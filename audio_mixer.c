
#include <uv.h>

#include "utils.h"
#include "avconv.h"
#include "audio_mixer.h"
#include "audio_out.h"

enum {
	TRACK_STOPPED,
	TRACK_PROBING,
	TRACK_PLAYING,
	TRACK_BLOCKING,
	TRACK_PAUSED,
	TRACK_PAUSING_VOL_DOWN,
	TRACK_RESUMING_VOL_UP,
	TRACK_FADING_OUT,
};

typedef struct {
} ringbuf_t;

static int ringbuf_space() {
}

typedef struct {
	avconv_t *av;
	int stat;
	ringbuf_t buf;
} audio_track_t;

typedef struct {
	audio_track_t tracks[2];
	audio_out_t *ao;
	uv_loop_t *loop;
	ringbuf_t mixbuf;
} audio_mixer_t;

/*
audio.stop = func () {
}
audio.pause = func () {
}
audio.resume = func () {
}
audio.play = func (url) {
}
*/

static audio_mixer_t *lua_getam(lua_State *L) {
	audio_mixer_t *am;
	void *ud = lua_touserdata(L, lua_upvalueindex(1));
	memcpy(&am, ud, sizeof(am));
	return am;
}

static int audio_play(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = &am->tracks[0];
	char *fname = (char *)lua_tostring(L, -1);

	if (tr->av) {
		avconv_stop(tr->av);
		tr->av = NULL;
	}

	tr->av = (avconv_t *)zalloc(sizeof(avconv_t));
	tr->av->data = tr;
	tr->av->on_probe = avconv_on_probe;
	tr->av->on_exit = avconv_on_exit;
	avconv_start(am->loop, tr->av, fname);

	tr->stat = TRACK_PROBING;

	return 0;
}

void audio_mixer_init(lua_State *L, uv_loop_t *loop) {
	audio_mixer_t *am = (audio_mixer_t *)zalloc(sizeof(audio_mixer_t));

	am->loop = loop;

	am->ao = (audio_out_t *)zalloc(sizeof(audio_out_t));
	audio_out_init(loop, am->ao, 44100);

	// audio = {}
	lua_newtable(L);
	lua_setglobal(L, "audio");

	// audio.play = [native function]
	lua_getglobal(L, "audio");
	void *ud = lua_newuserdata(L, sizeof(am));
	memcpy(ud, &am, sizeof(am));
	lua_pushcclosure(L, audio_play, 1);
	lua_setfield(L, -2, "play");
	lua_pop(L, 1);
}

