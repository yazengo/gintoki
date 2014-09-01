
#include <unistd.h>
#include <stdlib.h>

#include <uv.h>
#include <lua.h>
#include <pthread.h>

#include "utils.h"
#include "ringbuf.h"
#include "audio_in.h"
#include "audio_out_test.h"

#include <shairport.h>

typedef struct {
	lua_State *L;
	uv_loop_t *loop;

	ringbuf_t buf;
	shairport_t *sp;
	audio_in_t *ai;

	int stat;
	int rate;
} airplay_t;

enum {
	STOPPED, PLAYING,
};

static uv_loop_t *loop;
static lua_State *L;
static airplay_t _ap, *ap = &_ap;

// in main uv loop
static void play_put_buf_done(ringbuf_t *rb, int len) {
	void *pcall = rb->data;
	pthread_call_uv_complete(pcall);
}

// in main uv loop
static void play_put_buf(void *buf, int len, void *pcall) {
	debug("len=%d", len);
	ap->buf.data = pcall;
	ringbuf_data_put(&ap->buf, buf, len, play_put_buf_done);
}

// in main uv loop
static void play_get_buf_done(ringbuf_t *rb, int len) {
	debug("getbuf done");
	ap->ai->is_reading = 0;
	ap->ai->on_read_done(ap->ai, len);
}

// in main uv loop
static void play_get_buf(audio_in_t *ai, void *buf, int len) {
	debug("len=%d", len);
	ap->ai->is_reading = 1;
	ringbuf_data_get(&ap->buf, buf, len, play_get_buf_done);
}

static void play_stop(audio_in_t *ai) {
	debug("stopped");
	ringbuf_data_cancel_get(&ap->buf);
	ringbuf_data_cancel_put(&ap->buf);
	if (ap->ai == ai)
		ap->ai = NULL;
	if (ai->on_exit)
		ai->on_exit(ai);
	if (ai->on_free)
		ai->on_free(ai);
	ringbuf_init(&ap->buf);
}

void audio_in_airplay_init(uv_loop_t *loop, audio_in_t *ai) {
	if (ap->stat != PLAYING) {
		info("stopped");
		ai->on_free(ai);
		return;
	} 

	if (ap->ai)
		ap->ai->stop(ap->ai);

	ap->ai = ai;
	ai->read = play_get_buf;
	ai->stop = play_stop;
	ai->on_start(ai, ap->rate);
}

enum {
	START, STOP, PLAY,
};

typedef struct {
	int type;
	short *buf;
	int samples;
	int rate;
} shairport_cmd_t;

static void play_dummy_done(uv_timer_t *t, int _) {
	void *pcall = t->data;

	uv_timer_stop(t);
	pthread_call_uv_complete(pcall);
}

// in main uv loop
static void play_dummy(void *buf, int len, void *pcall) {
	debug("len=%d", len);

	if (ap->rate == 0)
		panic("rate == 0");

	int timeout = 1000 * len*1.0 / (ap->rate*4);

	static uv_timer_t t;
	t.data = pcall;
	uv_timer_init(ap->loop, &t);
	uv_timer_start(&t, play_dummy_done, timeout, timeout);
}

// in main uv loop
void call_airplay_start() {
	lua_getglobal(ap->L, "on_airplay_start");
	if (lua_isnil(ap->L, -1)) {
		lua_pop(ap->L, 1);
		return;
	}
	lua_call_or_die(ap->L, 0, 0);
}

// in main uv loop
static void on_shairport_cmd(void *pcall, void *_p) {
	shairport_cmd_t *c = (shairport_cmd_t *)_p;
	audio_in_t *ai = ap->ai;

	debug("stat=%d cmd=%d", ap->stat, c->type);

	if (ap->stat == STOPPED) {

		if (c->type == START) {
			ap->rate = c->rate;
			ap->stat = PLAYING;
			call_airplay_start();
		}

	} else if (ap->stat == PLAYING) {

		if (c->type == PLAY) {
			if (ai)
				play_put_buf(c->buf, c->samples*4, pcall);
			else
				play_dummy(c->buf, c->samples*4, pcall);
			return;
		}

		if (c->type == STOP) {
			ap->stat = STOPPED;
			debug("stopped");
			if (ai) 
				ai->stop(ai);
		}

	}

	pthread_call_uv_complete(pcall);
}

// in shairport thread 
static void on_shairport_start(int rate) {
	shairport_cmd_t c = {
		.type = START,
		.rate = rate,
	};
	pthread_call_uv_wait_withname(ap->loop, on_shairport_cmd, &c, "ap.start");
}

// in shairport thread 
static void on_shairport_stop() {
	shairport_cmd_t c = {
		.type = STOP,
	};
	pthread_call_uv_wait_withname(ap->loop, on_shairport_cmd, &c, "ap.stop");
}

// in shairport thread 
static void on_shairport_play(short buf[], int samples) {
	shairport_cmd_t c = {
		.type = PLAY,
		.buf = buf, .samples = samples,
	};
	pthread_call_uv_wait_withname(ap->loop, on_shairport_cmd, &c, "ap.play");
}

// in shairport thread 
static void *shairport_test_loop(void *_p) {
	shairport_t *sp = (shairport_t *)_p;
	static char buf[44100*4];
	int i;

	for (;;) {
		debug("airplay starts");
		sp->on_start(44100);
		for (i = 0; i < 4; i++) {
			audio_out_test_fill_buf_with_key(buf, sizeof(buf), 44100, i%7);
			debug("airplay test play len=%d", sizeof(buf));
			sp->on_play((short *)buf, sizeof(buf)/4); 
		}
		sp->on_stop();
		debug("airplay ends");
	}

	return NULL;
}

// in shairport thread 
static void *shairport_loop(void *_p) {
	shairport_t *sp = (shairport_t *)_p;

	info("airplay starts");

	shairport_start_loop(sp);

	return NULL;
}

void audio_in_airplay_start_loop(lua_State *L, uv_loop_t *loop) {
	ap->loop = loop;
	ap->L = L;

	ringbuf_init(&ap->buf);

	shairport_t *sp = (shairport_t *)zalloc(sizeof(shairport_t));
	sp->name = "Airplay on Muno";
	ap->sp = sp;

	sp->on_start = on_shairport_start;
	sp->on_stop = on_shairport_stop;
	sp->on_play = on_shairport_play;
	sp->data = ap;

	pthread_t tid;
	if (getenv("AIRPLAY_TEST"))
		pthread_create(&tid, NULL, shairport_test_loop, sp);
	else
		pthread_create(&tid, NULL, shairport_loop, sp);
}

static int lua_airplay_start(lua_State *L) {
	info("starts");
	audio_in_airplay_start_loop(L, loop);
	return 0;
}

void lua_airplay_init(lua_State *_L, uv_loop_t *_loop) {
	L = _L;
	loop = _loop;

	lua_pushcfunction(L, lua_airplay_start);
	lua_setglobal(L, "airplay_start");
}

