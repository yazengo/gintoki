
#include <string.h>

#include <uv.h>
#include <lua.h>

#include "utils.h"
#include "ringbuf.h"
#include "pcm.h"
#include "avconv.h"
#include "audio_mixer.h"
#include "audio_out.h"

#define PLAYBUF_SIZE (1024*4)
#define TRACKS_NR 2

enum {
	TRACK_STOPPED,
	TRACK_BUFFERING,
	TRACK_PLAYING,
	TRACK_PAUSED,
	TRACK_PAUSING_VOL_DOWN,
	TRACK_RESUMING_VOL_UP,
	TRACK_FADING_OUT,
};

struct audio_mixer_s;

typedef struct {
	struct audio_mixer_s *am;
	avconv_t *av;
	int stat;
	ringbuf_t buf;
	float vol;
	avconv_probe_t probe;
} audio_track_t;

typedef struct audio_mixer_s {
	audio_track_t tracks[TRACKS_NR];
	audio_out_t *ao;
	ringbuf_t mixbuf;
	float vol;

	uv_loop_t *loop;
	lua_State *L;
} audio_mixer_t;

static void check_all_tracks(audio_mixer_t *am);

// audio.emit(arg0, arg1)
static void audio_emit(audio_mixer_t *am, const char *arg0, const char *arg1) {
	lua_getglobal(am->L, "audio");
	lua_getfield(am->L, -1, "emit");
	lua_remove(am->L, -2);

	lua_pushstring(am->L, arg0);
	lua_pushstring(am->L, arg1);
	lua_call_or_die(am->L, 2, 0);
}

static void lua_call_play_done(audio_mixer_t *am, const char *stat) {
	char name[64];
	sprintf(name, "audio_mixer_play_done_%p", am);
	lua_getglobal(am->L, name);

	if (lua_isnil(am->L, -1)) {
		lua_pop(am->L, 1);
		return;
	}

	lua_pushstring(am->L, stat);
	lua_call_or_die(am->L, 1, 0);

	lua_pushnil(am->L);
	lua_setglobal(am->L, name);
}

static void lua_set_play_done(audio_mixer_t *am) {
	char name[64];
	sprintf(name, "audio_mixer_play_done_%p", am);
	lua_setglobal(am->L, name);
}

static void lua_pusham(lua_State *L, audio_mixer_t *am) {
	void *ud = lua_newuserdata(L, sizeof(am));
	memcpy(ud, &am, sizeof(am));
}

static audio_mixer_t *lua_getam(lua_State *L) {
	audio_mixer_t *am;
	void *ud = lua_touserdata(L, lua_upvalueindex(1));
	memcpy(&am, ud, sizeof(am));
	return am;
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
	tr->stat = stat;
	audio_emit(tr->am, "stat_change", track_stat_str(tr->stat));
}

static void avconv_on_exit(avconv_t *av) {
	audio_track_t *tr = (audio_track_t *)av->data;

	tr->av = NULL;
	track_change_stat(tr, TRACK_STOPPED);
	lua_call_play_done(tr->am, "done");
}

static void avconv_on_free(avconv_t *av) {
	free(av);
}

static void avconv_on_probed(avconv_t *av) {
	audio_track_t *tr = (audio_track_t *)av->data;

	tr->probe = av->probe;
}

static void avconv_on_read_done(avconv_t *av, int len) {
	audio_track_t *tr = (audio_track_t *)av->data;

	if (len < 0)
		return;

	ringbuf_push_head(&tr->buf, len);
	check_all_tracks(tr->am);
}

static void audio_out_on_play_done(audio_out_t *ao, int len) {
	audio_mixer_t *am = (audio_mixer_t *)ao->data;

	ringbuf_push_tail(&am->mixbuf, len);
	check_all_tracks(am);
}

static void check_all_tracks(audio_mixer_t *am) {
	int playlen = PLAYBUF_SIZE;
	audio_track_t *playtr[TRACKS_NR];
	int canplay = 0;
	int i;

	void *mixbuf; int mixlen;
	ringbuf_space_ahead_get(&am->mixbuf, &mixbuf, &mixlen);

	if (mixlen < playlen)
		playlen = mixlen;

	for (i = 0; i < TRACKS_NR; i++) {
		audio_track_t *tr = &am->tracks[i];

		if (tr->stat == TRACK_STOPPED)
			continue;

		void *spacebuf; int spacelen;
		ringbuf_space_ahead_get(&tr->buf, &spacebuf, &spacelen);
		if (spacelen > 0) {
			avconv_read(tr->av, spacebuf, spacelen, avconv_on_read_done);
		}

		void *databuf; int datalen;
		ringbuf_data_ahead_get(&tr->buf, &databuf, &datalen);
		if (datalen > 0) {
			playtr[canplay++] = tr;
			if (datalen < playlen)
				playlen = datalen;
		}
	}

	if (!canplay)
		return;

	for (i = 0; i < canplay; i++) {
		audio_track_t *tr = playtr[i];

		void *databuf; int datalen;
		ringbuf_data_ahead_get(&tr->buf, &databuf, &datalen);
		ringbuf_push_tail(&tr->buf, playlen);

		if (i == 0) 
			memcpy(mixbuf, databuf, playlen);
		else
			pcm_do_mix(mixbuf, databuf, playlen);
	}

	pcm_do_volume(mixbuf, playlen, am->vol);
	ringbuf_push_head(&am->mixbuf, playlen);
	audio_out_play(am->ao, mixbuf, playlen, audio_out_on_play_done);
}

// audio.play(url, done)
static int audio_play(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = &am->tracks[0];

	// -1 url
	if (!lua_isfunction(L, -1))
		lua_pushnil(L);

	// -1 done
	// -2 url
	char *fname = (char *)lua_tostring(L, -2);
	lua_set_play_done(am);
	lua_pop(L, 1);

	if (tr->av) {
		avconv_stop(tr->av);
		tr->av = NULL;
	}

	ringbuf_clear(&tr->buf);
	memset(&tr->probe, 0, sizeof(tr->probe));

	tr->am = am;
	tr->av = (avconv_t *)zalloc(sizeof(avconv_t));
	tr->av->data = tr;
	tr->av->on_probed = avconv_on_probed;
	tr->av->on_exit = avconv_on_exit;
	tr->av->on_free = avconv_on_free;
	avconv_start(am->loop, tr->av, fname);

	track_change_stat(tr, TRACK_BUFFERING);
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

	lua_pushnumber(L, (int)tr->probe.dur);
	lua_setfield(L, -2, "duration");

	lua_pushnumber(L, (int)track_get_pos(tr));
	lua_setfield(L, -2, "position");

	return 1;
}

void audio_mixer_init(lua_State *L, uv_loop_t *loop) {
	audio_mixer_t *am = (audio_mixer_t *)zalloc(sizeof(audio_mixer_t));
	am->loop = loop;
	am->L = L;

	am->vol = 1.0;

	am->ao = (audio_out_t *)zalloc(sizeof(audio_out_t));
	am->ao->data = am;
	audio_out_init(loop, am->ao, 44100);

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
}

