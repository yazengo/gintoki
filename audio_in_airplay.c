
#include <unistd.h>

#include <uv.h>
#include <lua.h>
#include <pthread.h>

#include "utils.h"
#include "ringbuf.h"
#include "audio_in.h"
#include "audio_out_test.h"

#include <shairport.h>

typedef struct {
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

static airplay_t _ap, *ap = &_ap;

// in airplay thread
static void play_on_put_done(ringbuf_t *rb, int len) {
	void *pcall = rb->data;
	pthread_call_uv_complete(pcall);
}

// in airplay thread
static void play_put_buf(void *buf, int len, void *pcall) {
	ap->buf.data = pcall;
	ringbuf_data_put(&ap->buf, buf, len, play_on_put_done);
}

// 
static void play_on_get_done(ringbuf_t *rb, int len) {
	ap->ai->on_read_done(ap->ai, len);
	ap->ai->on_read_done = NULL;
}

static void play_get_buf(audio_in_t *ai, void *buf, int len) {
	//audio_out_test_fill_buf_with_key(buf, len, 22050, 3);
	ringbuf_data_get(&ap->buf, buf, len, play_on_get_done);
}

static void play_stop(audio_in_t *ai) {
	ringbuf_data_cancel_get(&ap->buf);
	ringbuf_data_cancel_put(&ap->buf);
	if (ap->ai == ai)
		ap->ai = NULL;
	if (ai->on_exit)
		ai->on_exit(ai);
	if (ai->on_free)
		ai->on_free(ai);
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

static void play_dummy(void *buf, int len, void *pcall) {
	if (ap->rate)
		usleep(1e6 * len*1.0 / (ap->rate*4));
	pthread_call_uv_complete(pcall);
}

// in main uv loop
static void on_shairport_cmd(void *pcall, void *_p) {
	shairport_cmd_t *c = (shairport_cmd_t *)_p;
	audio_in_t *ai = ap->ai;

	if (ap->stat == STOPPED) {

		if (c->type == START) {
			ap->rate = c->rate;
			ap->stat = PLAYING;
			//emit("airplay.on_start");
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
	pthread_call_uv_wait(ap->loop, on_shairport_cmd, &c);
}

// in shairport thread 
static void on_shairport_stop() {
	shairport_cmd_t c = {
		.type = STOP,
	};
	pthread_call_uv_wait(ap->loop, on_shairport_cmd, &c);
}

// in shairport thread 
static void on_shairport_play(short buf[], int samples) {
	shairport_cmd_t c = {
		.type = PLAY,
		.buf = buf, .samples = samples,
	};
	pthread_call_uv_wait(ap->loop, on_shairport_cmd, &c);
}

// in shairport thread 
static void *shairport_test_loop(void *_p) {
	shairport_t *sp = (shairport_t *)_p;
	static char buf[44100*8];
	int i;

	for (;;) {
		info("airplay starts");
		sp->on_start(44100);
		for (i = 0; ; i++) {
			audio_out_test_fill_buf_with_key(buf, sizeof(buf), 44100, i%7);
			sp->on_play((short *)buf, sizeof(buf)/4); 
		}
		sp->on_stop();
		info("airplay ends");
	}

	return NULL;
}

void audio_in_airplay_start_loop(lua_State *L, uv_loop_t *loop) {
	shairport_t *sp = (shairport_t *)zalloc(sizeof(shairport_t));
	ap->sp = sp;

	sp->on_start = on_shairport_start;
	sp->on_stop = on_shairport_stop;
	sp->on_play = on_shairport_play;
	sp->data = ap;

	pthread_t tid;
	pthread_create(&tid, NULL, shairport_test_loop, sp);
}

