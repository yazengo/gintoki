
#include <string.h>

#include <uv.h>
#include <lua.h>

#include "utils.h"
#include "ringbuf.h"
#include "pcm.h"
#include "audio_in.h"
#include "audio_out.h"
#include "audio_mixer.h"

#define MAX_MIXLEN (1024*2)
#define MAX_TRACKBUF (1024*8)
#define TRACKS_NR 4

enum {
	TRACK_READING,
	TRACK_WAITING_EOF,
	TRACK_CLOSING,
	TRACK_CLOSED,
};

struct audio_mixer_s;

typedef struct {
	struct audio_mixer_s *am;
	int ti;

	audio_in_t *ai;
	int reading:1;

	int stat;

	int paused:1;
	int pending_close:1;

	ringbuf_t buf;
	float vol;
	float dur;

	int first_blood; // test only
} audio_track_t;

typedef struct audio_mixer_s {
	audio_track_t *tracks[TRACKS_NR];

	audio_out_t *ao;
	int playing;

	ringbuf_t mixbuf;

	float vol;
	int rate;

	uv_loop_t *loop;
	lua_State *L;

	int filter_track0_setvol;
	float track0_vol;
} audio_mixer_t;

static void audio_emit(audio_mixer_t *am, const char *arg0, const char *arg1);
static void mix_and_play(audio_mixer_t *am);
static void track_read(audio_track_t *tr);
static void track_on_closed(audio_track_t *tr);

static void lua_call_play_done(audio_track_t *tr, const char *stat) {
	lua_State *L = tr->am->L;

	lua_pushstring(L, stat);
	lua_do_global_callback(L, "audio_mixer_play_done", tr, 1, 1);
}

static void lua_set_play_done(audio_track_t *tr) {
	lua_State *L = tr->am->L;

	lua_set_global_callback(L, "audio_mixer_play_done", tr);
}

static void lua_pusham(lua_State *L, audio_mixer_t *am) {
	lua_pushuserptr(L, am);
}

static audio_mixer_t *lua_getam(lua_State *L) {
	return (audio_mixer_t *)lua_touserptr(L, lua_upvalueindex(1));
}

static void audio_in_on_closed(audio_in_t *ai) {
	audio_track_t *tr = (audio_track_t *)ai->data;

	info("closed");
	free(ai);

	if (tr->pending_close || tr->buf.len == 0)
		track_on_closed(tr);
	else
		tr->stat = TRACK_CLOSED;
}

static void audio_in_on_read_done(audio_in_t *ai, int len) {
	audio_track_t *tr = (audio_track_t *)ai->data;

	debug("len=%d", len);

	tr->reading = 0;

	if (len < 0 || tr->stat == TRACK_WAITING_EOF) {
		tr->stat = TRACK_CLOSING;
		tr->ai->close(tr->ai, audio_in_on_closed);
		return;
	}

	ringbuf_push_head(&tr->buf, len);
	track_read(tr);

	mix_and_play(tr->am);
}

static void audio_in_on_probe(audio_in_t *ai, const char *key, void *_val) {
	audio_track_t *tr = (audio_track_t *)ai->data;

	if (strcmp(key, "dur") == 0) {
		tr->dur = *(float *)_val;
		info("dur=%f", tr->dur);
	}
}

static void audio_out_on_play_done(audio_out_t *ao, int len) {
	audio_mixer_t *am = (audio_mixer_t *)ao->data;

	debug("len=%d", len);

	am->playing = 0;
	ringbuf_push_tail(&am->mixbuf, len);

	mix_and_play(am);
}

static float track_get_pos(audio_track_t *tr) {
	return (float)tr->buf.tailpos / (44100*2*2); // int16_t*2 per sample
}

static void track_on_closed(audio_track_t *tr) {
	audio_mixer_t *am = tr->am;

	info("closed");

	am->tracks[tr->ti] = NULL;
	lua_call_play_done(tr, "closed");
	free(tr);
}

static void track_close(audio_track_t *tr) {
	debug("stat=%d", tr->stat);

	switch (tr->stat) {
	case TRACK_READING:
		tr->ai->stop(tr->ai);

		if (tr->reading) {
			tr->pending_close = 1;
			tr->stat = TRACK_WAITING_EOF;
		} else {
			track_on_closed(tr);
		}
		break;

	case TRACK_WAITING_EOF:
	case TRACK_CLOSING:
		tr->pending_close = 1;
		break;

	case TRACK_CLOSED:
		track_on_closed(tr);
		break;
	}
}

static void track_read(audio_track_t *tr) {
	if (!(tr->stat == TRACK_READING && !tr->reading))
		return;

	void *buf; int len;
	ringbuf_space_ahead_get(&tr->buf, &buf, &len);
	if (len == 0)
		return;

	tr->reading = 1;
	tr->ai->read(tr->ai, buf, len, audio_in_on_read_done);
}

static void mix_and_play(audio_mixer_t *am) {
	if (am->playing)
		return;

	void *mixbuf; int mixlen;
	ringbuf_space_ahead_get(&am->mixbuf, &mixbuf, &mixlen);
	if (mixlen == 0)
		return;

	int max_mixlen = MAX_MIXLEN - am->mixbuf.len;
	if (mixlen > max_mixlen)
		mixlen = max_mixlen;
	if (mixlen <= 0)
		return;

	audio_track_t *trmix[TRACKS_NR];
	int canmix = 0;

	int i;
	for (i = 0; i < TRACKS_NR; i++) {
		audio_track_t *tr = am->tracks[i];

		if (!(tr && tr->buf.len > 0 && !tr->paused))
			continue;

		void *databuf; int datalen;
		ringbuf_data_ahead_get(&tr->buf, &databuf, &datalen);

		if (datalen < mixlen)
			mixlen = datalen;

		trmix[canmix++] = tr;
	}

	if (canmix == 0)
		return;

	for (i = 0; i < canmix; i++) {
		audio_track_t *tr = trmix[i];

		void *databuf; int datalen;
		ringbuf_data_ahead_get(&tr->buf, &databuf, &datalen);

		if (am->filter_track0_setvol && tr == am->tracks[0])
			pcm_do_volume(databuf, mixlen, am->track0_vol);

		if (i == 0)
			memcpy(mixbuf, databuf, mixlen);
		else
			pcm_do_mix(mixbuf, databuf, mixlen);

		ringbuf_push_tail(&tr->buf, mixlen);
		debug("mix#%d: stat=%d len=%d", i, tr->stat, tr->buf.len);

		if (tr->stat == TRACK_CLOSED && tr->buf.len == 0)
			track_on_closed(tr);
		else
			track_read(tr);
	}

	debug("mixlen=%d", mixlen);

	pcm_do_volume(mixbuf, mixlen, am->vol);
	ringbuf_push_head(&am->mixbuf, mixlen);

	am->playing = 1;
	audio_out_play(am->ao, mixbuf, mixlen, audio_out_on_play_done);
}

static int parse_track_i(lua_State *L) {
	if (!lua_istable(L, 1) || lua_isnil(L, 1))
		return 0;

	lua_getfield(L, 1, "track");
	int i = lua_tonumber(L, -1);
	lua_pop(L, 1);

	if (i > TRACKS_NR)
		i = TRACKS_NR-1;
	if (i < 0)
		i = 0;

	return i;
}

// audio.emit(arg0, arg1)
static void audio_emit(audio_mixer_t *am, const char *arg0, const char *arg1) {
	lua_getglobal(am->L, "audio");
	lua_getfield(am->L, -1, "emit");
	lua_remove(am->L, -2);

	lua_pushstring(am->L, arg0);
	lua_pushstring(am->L, arg1);
	lua_call_or_die(am->L, 2, 0);
}

// upvalue[1] = {url='filename',i=0/1,done=function}
static int audio_on_stopped(lua_State *L) {
	lua_getglobal(L, "audio");
	lua_getfield(L, -1, "play");
	lua_remove(L, -2);
	
	lua_pushvalue(L, lua_upvalueindex(1));

	lua_call_or_die(L, 1, 0);

	return 0;
}

// audio.play({url='filename',track=0/1,done=function})
static int audio_play(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);

	lua_getfield(L, 1, "url");
	char *url = (char *)lua_tostring(L, -1);
	if (url == NULL) 
		panic("url must set");

	int ti = parse_track_i(L);
	audio_track_t *tr = am->tracks[ti];

	if (tr) {
		debug("wait close");

		lua_pushvalue(L, 1);
		lua_pushcclosure(L, audio_on_stopped, 1);
		lua_set_play_done(tr);
		track_close(tr);
		return 0;
	}

	tr = (audio_track_t *)zalloc(sizeof(audio_track_t));
	tr->ti = ti;
	tr->am = am;
	am->tracks[ti] = tr;

	info("url=%s i=%d", url, ti);

	audio_in_t *ai = (audio_in_t *)zalloc(sizeof(audio_in_t));
	tr->ai = ai;
	ai->data = tr;
	ai->on_probe = audio_in_on_probe;
	ai->url = url;

	lua_getfield(L, 1, "done");
	lua_set_play_done(tr);

	audio_in_init(am->loop, ai);
	track_read(tr);

	return 0;
}

// audio.info() = {duration=123, position=123, stat='playing/paused/stopped/buffering'}
static int audio_info(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = am->tracks[0];

	lua_newtable(L);

	//TODO
	//lua_pushstring(L, track_stat_str(tr->stat));
	//lua_setfield(L, -2, "stat");

	lua_pushnumber(L, (int)track_get_pos(tr));
	lua_setfield(L, -2, "position");

	lua_pushnumber(L, (int)tr->dur);
	lua_setfield(L, -2, "duration");

	return 1;
}

// audio.setvol(vol) = 11
static int audio_setvol(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	int vol = lua_tonumber(L, -1);

	if (vol > 100)
		vol = 100;
	if (vol < 0)
		vol = 0;

	am->vol = vol/100.0;

	lua_pushnumber(L, (int)(am->vol*100));
	return 1;
}

// audio.getvol() = 11
static int audio_getvol(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	lua_pushnumber(L, (int)(am->vol*100));
	return 1;
}

// audio.pause{track=1}
static int audio_pause(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = am->tracks[parse_track_i(L)];

	if (tr)
		tr->paused = 1;

	return 0;
}

// audio.resume{track=1}
static int audio_resume(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = am->tracks[parse_track_i(L)];

	if (tr) {
		tr->paused = 0;
		mix_and_play(am);
	}

	return 0;
}

// audio.stop{track=1}
static int audio_stop(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	int ti = parse_track_i(L);
	audio_track_t *tr = am->tracks[ti];

	track_close(tr);

	return 0;
}

// audio.pause_resume_toggle{track=1}
static int audio_pause_resume_toggle(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = am->tracks[parse_track_i(L)];

	if (tr) {
		tr->paused = !tr->paused;
		mix_and_play(am);
	}

	return 0;
}

// audio.setopt{track0_setvol = true/false, vol = 20}
static int audio_setopt(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);

	lua_getfield(L, 1, "track0_setvol");
	if (!lua_isnil(L, -1)) {
		int on = lua_toboolean(L, -1);

		lua_getfield(L, 1, "vol");
		int vol = lua_tonumber(L, -1);
		
		am->filter_track0_setvol = on;
		am->track0_vol = (float)vol/100;

		info("track0_setvol=%d,%d", on, vol);
	}

	return 0;
}

void audio_mixer_init(lua_State *L, uv_loop_t *loop) {
	audio_mixer_t *am = (audio_mixer_t *)zalloc(sizeof(audio_mixer_t));
	am->loop = loop;
	am->L = L;

	am->vol = 1.0;

	am->ao = (audio_out_t *)zalloc(sizeof(audio_out_t));
	am->ao->data = am;
	am->rate = 44100;
	audio_out_init(loop, am->ao, am->rate);

	// audio = {}
	lua_newtable(L);
	lua_setglobal(L, "audio");

	// emitter_init(audio)
	lua_getglobal(L, "emitter_init");
	lua_getglobal(L, "audio");
	lua_call_or_die(L, 1, 0);

	// audio.stop = [native function]
	lua_getglobal(L, "audio");
	lua_pusham(L, am);
	lua_pushcclosure(L, audio_stop, 1);
	lua_setfield(L, -2, "stop");
	lua_pop(L, 1);

	// audio.play = [native function]
	lua_getglobal(L, "audio");
	lua_pusham(L, am);
	lua_pushcclosure(L, audio_play, 1);
	lua_setfield(L, -2, "play");
	lua_pop(L, 1);

	// audio.info = [native function]
	lua_getglobal(L, "audio");
	lua_pusham(L, am);
	lua_pushcclosure(L, audio_info, 1);
	lua_setfield(L, -2, "info");
	lua_pop(L, 1);

	// audio.getvol = [native function]
	lua_getglobal(L, "audio");
	lua_pusham(L, am);
	lua_pushcclosure(L, audio_getvol, 1);
	lua_setfield(L, -2, "getvol");
	lua_pop(L, 1);

	// audio.setvol = [native function]
	lua_getglobal(L, "audio");
	lua_pusham(L, am);
	lua_pushcclosure(L, audio_setvol, 1);
	lua_setfield(L, -2, "setvol");
	lua_pop(L, 1);

	// audio.pause = [native function]
	lua_getglobal(L, "audio");
	lua_pusham(L, am);
	lua_pushcclosure(L, audio_pause, 1);
	lua_setfield(L, -2, "pause");
	lua_pop(L, 1);

	// audio.resume = [native function]
	lua_getglobal(L, "audio");
	lua_pusham(L, am);
	lua_pushcclosure(L, audio_resume, 1);
	lua_setfield(L, -2, "resume");
	lua_pop(L, 1);

	// audio.pause_resume_toggle = [native function]
	lua_getglobal(L, "audio");
	lua_pusham(L, am);
	lua_pushcclosure(L, audio_pause_resume_toggle, 1);
	lua_setfield(L, -2, "pause_resume_toggle");
	lua_pop(L, 1);

	// audio.setopt = [native function]
	lua_getglobal(L, "audio");
	lua_pusham(L, am);
	lua_pushcclosure(L, audio_setopt, 1);
	lua_setfield(L, -2, "setopt");
	lua_pop(L, 1);
}

