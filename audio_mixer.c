
#include <string.h>

#include <uv.h>
#include <lua.h>

#include "utils.h"
#include "ringbuf.h"
#include "pcm.h"
#include "avconv.h"
#include "audio_mixer.h"
#include "audio_in.h"
#include "audio_out.h"

#define MAX_MIXLEN (2048)
#define TRACKS_NR 2

enum {
	TRACK_STOPPED,
	TRACK_BUFFERING,
	TRACK_PLAYING,
	TRACK_PAUSED,
};

enum {
	TRACK_CLOSE_FADING_OUT = 1,
	TRACK_OPEN_FADING_IN,
};

struct audio_mixer_s;

typedef struct {
	struct audio_mixer_s *am;
	audio_in_t *ai;
	int stat;
	ringbuf_t buf;
	float vol;
	float dur;

	int first_blood; // test only
} audio_track_t;

typedef struct audio_mixer_s {
	audio_track_t tracks[TRACKS_NR];
	audio_out_t *ao;
	ringbuf_t mixbuf;
	float vol;
	int rate;
	uv_loop_t *loop;
	lua_State *L;
} audio_mixer_t;

static void audio_emit(audio_mixer_t *am, const char *arg0, const char *arg1);
static void check_all_tracks(audio_mixer_t *am);

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

static const char *track_stat_str(int stat) {
	switch (stat) {
	case TRACK_STOPPED: return "stopped";
	case TRACK_BUFFERING: return "buffering";
	case TRACK_PLAYING: return "playing";
	case TRACK_PAUSED: return "paused";
	default: return "?";
	}
}

static float track_get_pos(audio_track_t *tr) {
	return (float)tr->buf.tailpos / (44100*2*2); // int16_t*2 per sample
}

static void track_change_stat(audio_track_t *tr, int stat) {
	if (tr->stat == stat)
		return;

	int send = 0;

	if (stat == TRACK_PAUSED || tr->stat == TRACK_PAUSED)
		send = 1;

	if (tr->stat == TRACK_BUFFERING && stat == TRACK_PLAYING) {
		if (tr->first_blood) {
			send = 1;
			tr->first_blood = 0;
		}
	}

	tr->stat = stat;
	if (send)
		audio_emit(tr->am, "stat_change", track_stat_str(stat));
}

static void audio_in_on_exit(audio_in_t *ai) {
	audio_track_t *tr = (audio_track_t *)ai->data;
	tr->ai = NULL;

	check_all_tracks(tr->am);
}

static void audio_in_on_free(audio_in_t *ai) {
	free(ai);
}

static void audio_in_on_probe(audio_in_t *ai, const char *key, void *_val) {
	audio_track_t *tr = (audio_track_t *)ai->data;

	if (strcmp(key, "dur") == 0) {
		tr->dur = *(float *)_val;
		info("dur=%f", tr->dur);
	}
}

static void audio_in_on_read_done(audio_in_t *ai, int len) {
	audio_track_t *tr = (audio_track_t *)ai->data;

	debug("done len=%d", len);

	ringbuf_push_head(&tr->buf, len);
	check_all_tracks(tr->am);
}

static void audio_out_on_play_done(audio_out_t *ao, int len) {
	audio_mixer_t *am = (audio_mixer_t *)ao->data;

	debug("playdone");

	ringbuf_push_tail(&am->mixbuf, len);
	check_all_tracks(am);
}

static void audio_in_on_start(audio_in_t *ai, int rate) {
	audio_track_t *tr = (audio_track_t *)ai->data;
	audio_mixer_t *am = tr->am;

	if (am->rate != rate) {
		info("rate -> %d", rate);
		audio_out_set_rate(am->ao, rate);
		am->rate = rate;
	}
}

static void check_tracks_can_close(audio_mixer_t *am) {
	int i;
	for (i = 0; i < TRACKS_NR; i++) {
		audio_track_t *tr = &am->tracks[i];

		if (!(tr->ai == NULL && tr->stat != TRACK_STOPPED && tr->buf.len == 0))
			continue;

		info("closed #%d", i);

		tr->stat = TRACK_STOPPED;
		lua_call_play_done(tr, "done");
	}
}

static void check_tracks_can_read(audio_mixer_t *am) {
	int i;
	for (i = 0; i < TRACKS_NR; i++) {
		audio_track_t *tr = &am->tracks[i];

		if (tr->ai == NULL || tr->stat == TRACK_STOPPED)
			continue;

		void *buf; int len;
		ringbuf_space_ahead_get(&tr->buf, &buf, &len);
		if (len > 0 && !audio_in_is_reading(tr->ai)) {
			audio_in_read(tr->ai, buf, len, audio_in_on_read_done);
		}
	}
}

static void check_tracks_can_mix(audio_mixer_t *am) {

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
		audio_track_t *tr = &am->tracks[i];

		if (tr->stat == TRACK_STOPPED || tr->stat == TRACK_PAUSED)
			continue;

		void *databuf; int datalen;
		ringbuf_data_ahead_get(&tr->buf, &databuf, &datalen);

		if (datalen == 0) {
			track_change_stat(tr, TRACK_BUFFERING);
			continue;
		} else 
			track_change_stat(tr, TRACK_PLAYING);

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
		if (i == 0)
			memcpy(mixbuf, databuf, mixlen);
		else
			pcm_do_mix(mixbuf, databuf, mixlen);
		ringbuf_push_tail(&tr->buf, mixlen);
	}

	debug("canmix=%d mixlen=%d", canmix, mixlen);

	pcm_do_volume(mixbuf, mixlen, am->vol);
	ringbuf_push_head(&am->mixbuf, mixlen);
}

static void check_tracks_can_play(audio_mixer_t *am) {
	void *databuf; int datalen;
	ringbuf_data_ahead_get(&am->mixbuf, &databuf, &datalen);
	if (datalen == 0)
		return;

	debug("playlen=%d", datalen);

	if (!audio_out_is_playing(am->ao)) 
		audio_out_play(am->ao, databuf, datalen, audio_out_on_play_done);
	else
		debug("playing");
}

static void check_all_tracks(audio_mixer_t *am) {
	check_tracks_can_close(am);
	check_tracks_can_read(am);
	check_tracks_can_mix(am);
	check_tracks_can_play(am);
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

// audio.play({url='filename',i=0/1,done=function})
static int audio_play(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);

	lua_getfield(L, 1, "i"); // 2
	lua_getfield(L, 1, "url"); // 3
	lua_getfield(L, 1, "done"); // 4

	int i = lua_tonumber(L, 2);
	if (i > TRACKS_NR)
		i = TRACKS_NR-1;
	if (i < 0)
		i = 0;

	char *url = (char *)lua_tostring(L, 3);
	if (url == NULL) {
		warn("failed: url=nil");
		return 0;
	}

	audio_track_t *tr = &am->tracks[i];
	tr->am = am;

	lua_pushvalue(L, 4);
	lua_set_play_done(tr);

	info("url=%s i=%d", url, i);

	audio_out_cancel_play(am->ao);

	if (tr->ai) {
		audio_in_stop(tr->ai);
		tr->ai = NULL;
	}

	ringbuf_init(&tr->buf);
	ringbuf_init(&am->mixbuf);

	tr->ai = (audio_in_t *)zalloc(sizeof(audio_in_t));
	tr->ai->data = tr;
	tr->ai->on_probe = audio_in_on_probe;
	tr->ai->on_exit = audio_in_on_exit;
	tr->ai->on_free = audio_in_on_free;
	tr->ai->on_start = audio_in_on_start;
	tr->ai->url = url;

	//if (strncmp(url, "airplay://", strlen("airplay://")))
	//	audio_in_airplay_init(am->loop, tr->ai);
	
	audio_in_avconv_init(am->loop, tr->ai);

	tr->stat = TRACK_BUFFERING;
	tr->first_blood = 1; // for testing

	check_all_tracks(am);

	return 0;
}

// audio.info() = {duration=123, position=123, stat='playing/paused/stopped/buffering'}
static int audio_info(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = &am->tracks[0];

	lua_newtable(L);

	lua_pushstring(L, track_stat_str(tr->stat));
	lua_setfield(L, -2, "stat");

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

	lua_pop(L, 1);
	lua_pushnumber(L, (int)(am->vol*100));
	return 1;
}

// audio.getvol() = 11
static int audio_getvol(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	lua_pushnumber(L, (int)(am->vol*100));
	return 1;
}

// audio.pause()
static int audio_pause(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = &am->tracks[0];

	if (tr->stat == TRACK_PLAYING || tr->stat == TRACK_BUFFERING)
		track_change_stat(tr, TRACK_PAUSED);

	return 0;
}

// audio.resume()
static int audio_resume(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = &am->tracks[0];

	if (tr->stat == TRACK_PAUSED)
		track_change_stat(tr, TRACK_PLAYING);

	check_all_tracks(am);

	return 0;
}

// audio.pause_resume_toggle()
static int audio_pause_resume_toggle(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = &am->tracks[0];

	if (tr->stat == TRACK_PAUSED)
		return audio_resume(L);
	else
		return audio_pause(L);

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
}

