
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

struct audio_mixer_s;

typedef struct {
	struct audio_mixer_s *am;
	int ti;

	audio_in_t *ai;

	int stat;
	int paused:1;

	ringbuf_t buf;
	float vol;
	float dur;
} audio_track_t;

enum {
	READING_BUF_EMPTY,
	READING_BUF_HALFFULL,
	WAITING_BUF_FULL,
	WAITING_EOF,
	CLOSING_BUF_HALFFULL,
	CLOSING_BUF_EMPTY,
	CLOSED_BUF_HALFFULL,
	CLOSED_BUF_EMPTY,
};

#define FILTERS_NR 4

typedef struct {
	int type;
	float vol;
	int i;
} audio_filter_t;

enum {
	NONE,
	HIGHLIGHT,
};

typedef struct audio_mixer_s {
	audio_track_t *tracks[TRACKS_NR];

	int stat;

	audio_out_t *ao;

	ringbuf_t mixbuf;

	float vol;
	int rate;

	uv_loop_t *loop;
	lua_State *L;

	audio_filter_t filters[FILTERS_NR];
} audio_mixer_t;

enum {
	STOPPED,
	PLAYING,
};

typedef struct {
	audio_track_t *tr[TRACKS_NR];
	int n;
	void *buf;
	int len;
} do_mix_t;

static void lua_call_play_done(audio_track_t *tr, const char *stat);
static void lua_track_stat_change(audio_track_t *tr);
static void mixer_do_track_change(audio_mixer_t *am, do_mix_t *dm);
static void mixer_play(audio_mixer_t *am);
static void mixer_on_newdata(audio_mixer_t *am);
static void track_read(audio_track_t *tr);
static void track_close(audio_track_t *tr);

static float vol2fvol(int v) {
	if (v < 0)
		v = 0;
	else if (v > 100)
		v = 100;
	return v/100.0;
}

static int fvol2vol(float v) {
	return v*100;
}

static void track_on_free(audio_track_t *tr) {
	audio_mixer_t *am = tr->am;

	info("closed");

	am->tracks[tr->ti] = NULL;
	lua_call_play_done(tr, "closed");
	free(tr);
}

static void track_on_buf_full(audio_track_t *tr) {
	audio_mixer_t *am = tr->am;

	switch (tr->stat) {
	case READING_BUF_EMPTY:
		tr->stat = WAITING_BUF_FULL;
		lua_track_stat_change(tr);
		mixer_on_newdata(am);
		break;

	case READING_BUF_HALFFULL:
		tr->stat = WAITING_BUF_FULL;
		break;

	default:
		panic("stat=%d invalid", tr->stat);
	}
}

static void track_on_buf_halffull(audio_track_t *tr) {
	audio_mixer_t *am = tr->am;

	switch (tr->stat) {
	case READING_BUF_EMPTY:
		tr->stat = READING_BUF_HALFFULL;
		lua_track_stat_change(tr);
		mixer_on_newdata(am);
		break;

	case WAITING_BUF_FULL:
	case READING_BUF_HALFFULL:
		tr->stat = READING_BUF_HALFFULL;
		track_read(tr);
		break;

	case CLOSING_BUF_HALFFULL:
	case CLOSED_BUF_HALFFULL:
		break;

	default:
		panic("stat=%d invalid", tr->stat);
	}
}

static void track_on_buf_empty(audio_track_t *tr) {
	switch (tr->stat) {
	case READING_BUF_EMPTY:
	case WAITING_BUF_FULL:
	case READING_BUF_HALFFULL:
		tr->stat = READING_BUF_EMPTY;
		track_read(tr);
		break;

	case CLOSING_BUF_HALFFULL:
		tr->stat = CLOSING_BUF_EMPTY;
		lua_track_stat_change(tr);
		break;

	case CLOSED_BUF_HALFFULL:
		track_on_free(tr);
		break;

	default:
		panic("stat=%d invalid", tr->stat);
	}
}

static void track_on_eof(audio_track_t *tr) {
	switch (tr->stat) {
	case WAITING_EOF:
	case READING_BUF_EMPTY:
		tr->stat = CLOSING_BUF_EMPTY;
		track_close(tr);
		break;

	case READING_BUF_HALFFULL:
		tr->stat = CLOSING_BUF_HALFFULL;
		track_close(tr);
		break;

	default:
		panic("stat=%d invalid", tr->stat);
	}
}

static void track_on_buf_change(audio_track_t *tr) {
	if (tr->buf.len == RINGBUF_SIZE)
		track_on_buf_full(tr);
	else if (tr->buf.len > 0)
		track_on_buf_halffull(tr);
	else
		track_on_buf_empty(tr);
}

static void audio_in_on_closed(audio_in_t *ai) {
	audio_track_t *tr = (audio_track_t *)ai->data;

	switch (tr->stat) {
	case CLOSING_BUF_EMPTY:
		track_on_free(tr);
		break;

	case CLOSING_BUF_HALFFULL:
		tr->stat = CLOSED_BUF_HALFFULL;
		break;

	default:
		panic("stat=%d invalid", tr->stat);
	}
}

static void audio_in_on_read_done(audio_in_t *ai, int len) {
	audio_track_t *tr = (audio_track_t *)ai->data;

	debug("len=%d", len);
	if (len < 0) {
		track_on_eof(tr);
		return;
	}

	ringbuf_push_head(&tr->buf, len);
	track_on_buf_change(tr);
}

static void audio_in_on_meta(audio_in_t *ai, const char *key, void *_val) {
	audio_track_t *tr = (audio_track_t *)ai->data;

	if (strcmp(key, "dur") == 0) {
		tr->dur = *(float *)_val;
		info("dur=%f", tr->dur);
	}
}

static float track_get_pos(audio_track_t *tr) {
	return (float)tr->buf.tailpos / (44100*2*2); // int16_t*2 per sample
}

static void track_close(audio_track_t *tr) {
	tr->ai->close(tr->ai, audio_in_on_closed);
}

static void track_pause(audio_track_t *tr) {
	audio_mixer_t *am = tr->am;

	switch (tr->stat) {
	case READING_BUF_EMPTY:
	case WAITING_BUF_FULL:
	case READING_BUF_HALFFULL:
	case CLOSING_BUF_HALFFULL:
	case CLOSED_BUF_HALFFULL:
		tr->paused = 1;
		lua_track_stat_change(tr);
		break;
	}
}

static void track_resume(audio_track_t *tr) {
	if (tr->paused)
		return;

	switch (tr->stat) {
	case READING_BUF_EMPTY:
		tr->paused = 0;
		lua_track_stat_change(tr);
		break;

	case WAITING_BUF_FULL:
	case READING_BUF_HALFFULL:
	case CLOSING_BUF_HALFFULL:
	case CLOSED_BUF_HALFFULL:
		tr->paused = 0;
		mixer_on_newdata(tr->am);
		lua_track_stat_change(tr);
		break;
	}
}

static void track_pause_resume_toggle(audio_track_t *tr) {
	if (tr->paused)
		track_resume(tr);
	else
		track_pause(tr);
}

static void track_stop(audio_track_t *tr) {
	switch (tr->stat) {
	case READING_BUF_EMPTY:
	case READING_BUF_HALFFULL:
		tr->ai->stop(tr->ai);
		ringbuf_init(&tr->buf);
		tr->stat = WAITING_EOF;
		break;

	case WAITING_BUF_FULL:
	case CLOSING_BUF_HALFFULL:
		ringbuf_init(&tr->buf);
		tr->stat = CLOSING_BUF_EMPTY;
		break;

	case CLOSED_BUF_HALFFULL:
		track_on_free(tr);
		break;
	}
}

static const char *track_statstr(audio_track_t *tr) {
	if (tr == NULL)
		return "stopped";

	switch (tr->stat) {
	case READING_BUF_EMPTY:
		return tr->paused ? "paused" : "buffering";

	case READING_BUF_HALFFULL:
	case WAITING_BUF_FULL:
	case CLOSING_BUF_HALFFULL:
	case CLOSED_BUF_HALFFULL:
		return tr->paused ? "paused" : "playing";

	case CLOSING_BUF_EMPTY:
	case WAITING_EOF:
		return "stopped";
	}

	return "?";
}

static void track_read(audio_track_t *tr) {
	void *buf; int len;
	ringbuf_space_ahead_get(&tr->buf, &buf, &len);
	tr->ai->read(tr->ai, buf, len, audio_in_on_read_done);
}

static void mixer_pre_mix(audio_mixer_t *am, do_mix_t *dm) {
	ringbuf_space_ahead_get(&am->mixbuf, &dm->buf, &dm->len);
	if (dm->len == 0)
		return;

	int maxlen = MAX_MIXLEN - am->mixbuf.len;
	if (dm->len > maxlen)
		dm->len = maxlen;

	if (dm->len <= 0)
		return;

	int i;
	for (i = 0; i < TRACKS_NR; i++) {
		audio_track_t *tr = am->tracks[i];

		if (!(tr && tr->buf.len > 0 && !tr->paused))
			continue;

		void *buf; int len;
		ringbuf_data_ahead_get(&tr->buf, &buf, &len);

		if (len < dm->len)
			dm->len = len;

		dm->tr[dm->n++] = tr;
	}
}

static void mixer_do_mix(audio_mixer_t *am, do_mix_t *dm) {
	int i;

	audio_filter_t *highlight = NULL;
	for (i = 0; i < FILTERS_NR; i++) {
		audio_filter_t *f = &am->filters[i];
		if (f->type == HIGHLIGHT && am->tracks[f->i])
			highlight = f;
	}

	for (i = 0; i < dm->n; i++) {
		audio_track_t *tr = dm->tr[i];

		void *buf; int len;
		ringbuf_data_ahead_get(&tr->buf, &buf, &len);

		if (highlight && tr != am->tracks[highlight->i])
			pcm_do_volume(buf, dm->len, highlight->vol);

		if (i == 0)
			memcpy(dm->buf, buf, len);
		else
			pcm_do_mix(dm->buf, buf, len);

		ringbuf_push_tail(&tr->buf, len);
		debug("mix#%d: stat=%d left=%d", i, tr->stat, tr->buf.len);
	}

	pcm_do_volume(dm->buf, dm->len, am->vol);

	debug("mixbuf: len=%d pushhead=%d", am->mixbuf.len, dm->len);
	ringbuf_push_head(&am->mixbuf, dm->len);
}

static void mixer_do_track_change(audio_mixer_t *am, do_mix_t *dm) {
	int i;

	for (i = 0; i < dm->n; i++) {
		audio_track_t *tr = dm->tr[i];
		track_on_buf_change(tr);
	}
}

static void mixer_on_newdata(audio_mixer_t *am) {
	if (am->stat == STOPPED)
		mixer_play(am);
}

static void mixer_on_play_done(audio_out_t *ao, int len) {
	audio_mixer_t *am = (audio_mixer_t *)ao->data;

	debug("len=%d", len);
	am->stat = STOPPED;

	debug("mixbuf: len=%d pushtail=%d", am->mixbuf.len, len);
	ringbuf_push_tail(&am->mixbuf, len);
	mixer_play(am);
}

static void mixer_play(audio_mixer_t *am) {
	if (am->stat != STOPPED)
		panic("stat=%d invalid", am->stat);

	do_mix_t dm = {};

	mixer_pre_mix(am, &dm);
	debug("n=%d len=%d", dm.n, dm.len);
	if (dm.n == 0) 
		return;
	mixer_do_mix(am, &dm);
	mixer_do_track_change(am, &dm);

	am->stat = PLAYING;
	audio_out_play(am->ao, dm.buf, dm.len, mixer_on_play_done);
}

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

static int lua_parse_track_i(lua_State *L) {
	if (!lua_istable(L, 1) || lua_isnil(L, 1))
		return 0;

	lua_getfield(L, 1, "track");
	int i = lua_tonumber(L, -1);
	lua_pop(L, 1);

	if (i > TRACKS_NR)
		i = TRACKS_NR-1;
	else if (i < 0)
		i = 0;

	return i;
}

// audio.track_stat_change(3) -> track#3 stat changed
static void lua_track_stat_change(audio_track_t *tr) {
	lua_State *L = tr->am->L;

	lua_getglobal(L, "audio");
	lua_getfield(L, -1, "track_stat_change");
	lua_remove(L, -2);

	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	lua_pushnumber(L, tr->ti);
	lua_call_or_die(L, 1, 0);
}

// upvalue[1] = [args of audio.play]
static int lua_audio_on_stopped(lua_State *L) {
	lua_getglobal(L, "audio");
	lua_getfield(L, -1, "play");
	lua_remove(L, -2);
	
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_call_or_die(L, 1, 0);

	return 0;
}

// audio.play {url='filename',track=0/1,done=function}
static int lua_audio_play(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);

	lua_getfield(L, 1, "url");
	char *url = (char *)lua_tostring(L, -1);
	if (url == NULL) 
		panic("url must set");

	int ti = lua_parse_track_i(L);
	audio_track_t *tr = am->tracks[ti];

	if (tr) {
		debug("wait close");

		lua_pushvalue(L, 1);
		lua_pushcclosure(L, lua_audio_on_stopped, 1);
		lua_set_play_done(tr);

		track_stop(tr);
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
	ai->on_meta = audio_in_on_meta;
	ai->url = url;

	lua_getfield(L, 1, "done");
	lua_set_play_done(tr);

	audio_in_init(am->loop, ai);

	tr->stat = READING_BUF_EMPTY;
	track_read(tr);
	lua_track_stat_change(tr);

	return 0;
}

// audio.info{track=1} -> {duration=123, position=123, stat='playing/paused/stopped/buffering'}
static int lua_audio_info(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = am->tracks[lua_parse_track_i(L)];

	lua_newtable(L);

	lua_pushstring(L, track_statstr(tr));
	lua_setfield(L, -2, "stat");

	lua_pushnumber(L, (int)track_get_pos(tr));
	lua_setfield(L, -2, "position");

	lua_pushnumber(L, (int)tr->dur);
	lua_setfield(L, -2, "duration");

	return 1;
}

// audio.setvol(vol) = 11
static int lua_audio_setvol(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	am->vol = vol2fvol(lua_tonumber(L, 1));
	lua_pushnumber(L, fvol2vol(am->vol));
	return 1;
}

// audio.getvol() = 11
static int lua_audio_getvol(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	lua_pushnumber(L, fvol2vol(am->vol));
	return 1;
}

// audio.pause{track=1}
static int lua_audio_pause(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = am->tracks[lua_parse_track_i(L)];

	if (tr)
		track_pause(tr);

	return 0;
}

// audio.resume{track=1}
static int lua_audio_resume(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = am->tracks[lua_parse_track_i(L)];

	if (tr)
		track_resume(tr);

	return 0;
}

// audio.pause_resume_toggle{track=1}
static int lua_audio_pause_resume_toggle(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = am->tracks[lua_parse_track_i(L)];

	if (tr)
		track_pause_resume_toggle(tr);

	return 0;
}

// audio.stop{track=1}
static int lua_audio_stop(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = am->tracks[lua_parse_track_i(L)];

	if (tr)
		track_stop(tr);

	return 0;
}

// audio.setfilter{enabled=false, slot=0, type='highlight', i=3, vol=20}
static int lua_audio_setfilter(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);

	lua_getfield(L, 1, "slot");
	int slot = lua_tonumber(L, -1);

	lua_getfield(L, 1, "type");
	const char *type = lua_tostring(L, -1);

	lua_getfield(L, 1, "enabled");
	int enabled = lua_toboolean(L, -1);

	if (slot < 0 || slot >= FILTERS_NR)
		panic("slot=%d invalid", slot);

	audio_filter_t *f = &am->filters[slot];

	if (!enabled) {
		f->type = NONE;
	} else {
		if (!strcmp(type, "highlight")) {
			f->type = HIGHLIGHT;

			lua_getfield(L, 1, "vol");
			f->vol = vol2fvol(lua_tonumber(L, -1));

			lua_getfield(L, 1, "i");
			f->i = lua_tonumber(L, -1);

			info("highlight: i=%d vol=%f", f->i, f->vol);
		}
	}

	return 0;
}

static struct {
	const char *name;
	lua_CFunction func;
} funcs[] = {
	{"play", lua_audio_play},
	{"stop", lua_audio_stop},
	{"info", lua_audio_info},
	{"getvol", lua_audio_getvol},
	{"setvol", lua_audio_setvol},
	{"pause", lua_audio_pause},
	{"resume", lua_audio_resume},
	{"pause_resume_toggle", lua_audio_pause_resume_toggle},
	{"setfilter", lua_audio_setfilter},
	{},
};

void luv_audio_mixer_init(lua_State *L, uv_loop_t *loop) {
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

	int i;
	for (i = 0; funcs[i].name; i++) {
		lua_getglobal(L, "audio");
		lua_pusham(L, am);
		lua_pushcclosure(L, funcs[i].func, 1);
		lua_setfield(L, -2, funcs[i].name);
		lua_pop(L, 1);
	}
}

