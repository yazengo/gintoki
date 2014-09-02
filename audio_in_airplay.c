
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
	int stat;
	void *wait;

	shairport_t *sp;
	int rate;
} airplay_srv_t;

typedef void (*write_done_cb)(int len, void *_p);

typedef struct {
	int stat;
	audio_in_t *ai;

	write_done_cb on_write_done;
	void *data;

	audio_in_read_cb on_read_done;
	audio_in_close_cb on_closed;

	ringbuf_t buf;
} airplay_cli_t;

enum {
	SRV_STOPPED,
	SRV_PLAYING,
};

<<<<<<< HEAD
enum {
	CLI_STOPPED,
	CLI_INIT,
	CLI_READING,
	CLI_CLOSING,
};
=======
static uv_loop_t *loop;
static lua_State *L;
static airplay_t _ap, *ap = &_ap;
>>>>>>> master

static airplay_srv_t _srv, *srv = &_srv;
static airplay_cli_t *cli;

static void cli_write(void *buf, int len, write_done_cb done, void *p);
static void cli_stop();

static void on_srv_start() {
	if (cli == NULL)
		return;

	if (cli->stat == CLI_STOPPED) {
		cli->ai->on_start(cli->ai, srv->rate);
		cli->stat = CLI_INIT;
	}
}

typedef struct {
	int len;
	write_done_cb done;
	void *p;
} dummy_write_t;

static void dummy_write_done(uv_timeout_t *to) {
	dummy_write_t *w = (dummy_write_t *)to->data;
	w->done(w->len, w->p);
}

static void dummy_write(void *buf, int len, write_done_cb done, void *wait) {
	static dummy_write_t w;
	w.done = done;
	w.len = len;
	w.p = wait;

	if (srv->rate == 0)
		panic("invalid sample rate");

	static uv_timeout_t to;
	to.data = &w;
	to.timeout = (int)(len / 4.0 / srv->rate * 1000);
	to.timeout_cb = dummy_write_done;

	uv_set_timeout(srv->loop, &to);
}

static void on_srv_write_done(int len, void *wait) {
	pthread_call_uv_complete(wait);
}

// in main uv loop
static void on_srv_write(void *buf, int len, void *wait) {
	if (cli) 
		cli_write(buf, len, on_srv_write_done, wait);
	else
		dummy_write(buf, len, on_srv_write_done, wait);
}

static void on_srv_stop() {
	if (cli)
		cli_stop();
}

static void cli_ringbuf_put_done(ringbuf_t *rb, int len) {
	cli->on_write_done(len, cli->data);
}

static void cli_write(void *buf, int len, write_done_cb done, void *p) {
	cli->on_write_done = done;
	cli->data = p;
	ringbuf_data_put(&cli->buf, buf, len, cli_ringbuf_put_done);
}

static void cli_stop() {
	switch (cli->stat) {
	case CLI_INIT:
	case CLI_READING:
		ringbuf_data_cancel_put(&cli->buf);
		ringbuf_data_cancel_get(&cli->buf);
		cli->stat = CLI_CLOSING;
		break;
	case CLI_CLOSING:
		break;
	default:
		panic("invalid stat=%d", cli->stat);
	}
}

static void cli_ringbuf_get_done(ringbuf_t *rb, int len) {
	switch (cli->stat) {
	case CLI_READING:
	case CLI_CLOSING:
		cli->stat = CLI_INIT;
		break;
	default:
		panic("stat=%d invalid: must be READING or CLOSING", cli->stat);
	}
	cli->on_read_done(cli->ai, len);
}

static void audio_in_read(audio_in_t *ai, void *buf, int len, audio_in_read_cb done) {
	if (cli->stat != CLI_INIT)
		panic("stat=%d invalid: must be INIT", cli->stat);

	cli->stat = CLI_READING;
	cli->on_read_done = done;
	ringbuf_data_get(&cli->buf, buf, len, cli_ringbuf_get_done);
}

static void audio_in_stop(audio_in_t *ai) {
	cli_stop();
}

static void audio_in_close_called(uv_call_t *c) {
	audio_in_t *ai = (audio_in_t *)c->data;
	free(c);

	cli->on_closed(ai);
	free(cli);
	cli = NULL;
	info("closed");
}

static void audio_in_close(audio_in_t *ai, audio_in_close_cb done) {
	if (cli->stat != CLI_CLOSING)
		panic("call stop() and check is_eof() before close()");
	cli->on_closed = done;

	uv_call_t *c = (uv_call_t *)zalloc(sizeof(uv_call_t));
	c->data = ai;
	c->done_cb = audio_in_close_called;
	uv_call(srv->loop, c);
}

static int audio_in_is_eof(audio_in_t *ai) {
	return cli->stat > CLI_READING;
}

static int audio_in_can_read(audio_in_t *ai) {
	return cli->stat == CLI_INIT;
}

void audio_in_airplay_init(uv_loop_t *loop, audio_in_t *ai) {
	if (cli)
		panic("only one airplay audio_in can exist. stat=%d", cli->stat);

	ai->read = audio_in_read;
	ai->stop = audio_in_stop;
	ai->close = audio_in_close;

	ai->is_eof = audio_in_is_eof;
	ai->can_read = audio_in_can_read;

	cli = (airplay_cli_t *)zalloc(sizeof(airplay_cli_t));
	cli->ai = ai;

	if (srv->stat != SRV_PLAYING) {
		cli->stat = CLI_CLOSING;
	} else {
		cli->stat = CLI_INIT;
		ai->on_start(ai, srv->rate);
	}

	ringbuf_init(&cli->buf, srv->loop);
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

// in main uv loop
void lua_call_airplay_start() {
	lua_getglobal(srv->L, "on_airplay_start");
	if (lua_isnil(srv->L, -1)) {
		lua_pop(srv->L, 1);
		return;
	}
	lua_call_or_die(srv->L, 0, 0);
}

// in main uv loop
static void on_shairport_cmd(void *pcall, void *_p) {
	shairport_cmd_t *c = (shairport_cmd_t *)_p;

	debug("stat=%d cmd=%d", srv->stat, c->type);

	if (srv->stat == SRV_STOPPED) {
		if (c->type == START) {
			srv->rate = c->rate;
			srv->stat = SRV_PLAYING;
			lua_call_airplay_start();
		}
	} else if (srv->stat == SRV_PLAYING) {
		if (c->type == PLAY) {
			on_srv_write(c->buf, c->samples*4, pcall);
			return;
		}
		if (c->type == STOP) {
			on_srv_stop();
			srv->stat = SRV_STOPPED;
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
	pthread_call_uv_wait_withname(srv->loop, on_shairport_cmd, &c, "ap.start");
}

// in shairport thread 
static void on_shairport_stop() {
	shairport_cmd_t c = {
		.type = STOP,
	};
	pthread_call_uv_wait_withname(srv->loop, on_shairport_cmd, &c, "ap.stop");
}

// in shairport thread 
static void on_shairport_play(short buf[], int samples) {
	shairport_cmd_t c = {
		.type = PLAY,
		.buf = buf, .samples = samples,
	};
	pthread_call_uv_wait_withname(srv->loop, on_shairport_cmd, &c, "ap.play");
}

// in shairport thread 
static void *shairport_test_loop(void *_p) {
	shairport_t *sp = (shairport_t *)_p;
	static char buf[44100/10];
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
	srv->loop = loop;
	srv->L = L;

	shairport_t *sp = (shairport_t *)zalloc(sizeof(shairport_t));
	sp->name = "Airplay on Muno";
	srv->sp = sp;

	sp->on_start = on_shairport_start;
	sp->on_stop = on_shairport_stop;
	sp->on_play = on_shairport_play;
	sp->data = srv;

	info("airplay starts '%s'", sp->name);

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

