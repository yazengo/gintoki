
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include <lua.h>

#include "utils.h"
#include "audio_in.h"

typedef struct {
	int stat;
	char *name;

	audio_in_t *ai;

	uv_pipe_t *pipe[2];
	uv_process_t *proc;

	lua_State *L;
	uv_loop_t *loop;

	uv_buf_t data_buf;

	audio_in_read_cb on_read_done;
	audio_in_close_cb on_close;
} airplay_t;

enum {
	INIT,
	STARTED,

	ATTACHED,
	READING,

	STOPPING,
	CLOSING_FD1,
	CLOSING_PROC,
};

static airplay_t *g_ap;

static void proc_start(airplay_t *ap);
static void audio_in_stop(audio_in_t *ai);

static void on_handle_closed(uv_handle_t *h) {
	airplay_t *ap = (airplay_t *)h->data;
	free(h);

	debug("stat=%d", ap->stat);

	switch (ap->stat) {
	case CLOSING_FD1:
		ap->stat = CLOSING_PROC;
		uv_close((uv_handle_t *)ap->proc, on_handle_closed);
		break;

	case CLOSING_PROC:
		ap->on_close(ap->ai);
		proc_start(ap);
		break;
	}
}

static uv_buf_t first_data_alloc_buffer(uv_handle_t *h, size_t len) {
	airplay_t *ap = (airplay_t *)h->data;
	static char buf[4];

	return uv_buf_init(buf, sizeof(buf));
}

static void first_data_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	airplay_t *ap = (airplay_t *)st->data;

	debug("first data comes");

	uv_read_stop(st);
	ap->stat = STARTED;

	lua_getglobal(ap->L, "airplay_on_start");
	if (!lua_isnil(ap->L, -1)) {
		lua_call_or_die(ap->L, 0, 0);
	} else 
		lua_pop(ap->L, 1);
}

static uv_buf_t data_alloc_buffer(uv_handle_t *h, size_t len) {
	airplay_t *ap = (airplay_t *)h->data;

	return ap->data_buf;
}

static void data_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	airplay_t *ap = (airplay_t *)st->data;
	audio_in_t *ai = ap->ai;

	uv_read_stop(st);

	debug("n=%d", n);

	switch (ap->stat) {
	case READING:
		if (n > 0) {
			ap->stat = ATTACHED;
			ap->on_read_done(ai, n);
		} else {
			ap->stat = STOPPING;
			ap->on_read_done(ai, 0);
		}
		break;

	case STOPPING:
		ap->on_read_done(ai, 0);
		break;

	default:
		panic("stat=%d invalid. must in READING,KILLING", ap->stat);
	}
}

static void proc_on_exit(uv_process_t *p, int stat, int sig) {
	airplay_t *ap = (airplay_t *)p->data;

	info("sig=%d", sig);
}

static void proc_start(airplay_t *ap) {
	uv_process_t *proc = (uv_process_t *)zalloc(sizeof(uv_process_t));
	proc->data = ap;
	ap->proc = proc;

	int i;
	for (i = 0; i < 1; i++) {
		ap->pipe[i] = (uv_pipe_t *)zalloc(sizeof(uv_pipe_t));
		ap->pipe[i]->data = ap;
		uv_pipe_init(ap->loop, ap->pipe[i], 0);
	}

	uv_stdio_container_t stdio[5] = {
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE|UV_WRITABLE_PIPE, .data.stream = (uv_stream_t *)ap->pipe[0]},
	};

	char *args_test[] = {getenv("_"), "-t", "110", NULL};
	char *args_shairport[] = {"shairport", "-a", ap->name, "-o", "pipe", NULL};
	char **args = args_shairport;

	if (getenv("AIRPLAY_TEST")) {
		info("running test");
		args = args_test;
	}

	uv_process_options_t opts = {
		.stdio = stdio,
		.stdio_count = 5,
		.file = args[0],
		.args = args,
		.exit_cb = proc_on_exit,
	};

	int r = uv_spawn(ap->loop, proc, opts);
	info("proc=%s spawn=%d pid=%d", args[0], r, proc->pid);

	ap->stat = INIT;
	uv_read_start((uv_stream_t *)ap->pipe[0], first_data_alloc_buffer, first_data_pipe_read);
}

// arg[1] = name
static int airplay_start(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));
	char *name = (char *)lua_tostring(L, 1);

	if (name == NULL)
		panic("name must be set");
	name = strdup(name);

	if (g_ap) {
		info("do restart");

		free(g_ap->name);
		g_ap->name = name;
		audio_in_stop(g_ap->ai);
		return 0;
	}

	g_ap = (airplay_t *)zalloc(sizeof(airplay_t));
	g_ap->L = L;
	g_ap->loop = loop;
	g_ap->name = name;

	proc_start(g_ap);

	return 0;
}

void luv_airplay_init(lua_State *L, uv_loop_t *loop) {
	// airplay_start = [native function]
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, airplay_start, 1);
	lua_setglobal(L, "airplay_start");
}

static void audio_in_read(audio_in_t *ai, void *buf, int len, audio_in_read_cb done) {
	airplay_t *ap = (airplay_t *)ai->in;

	if (ap->stat != ATTACHED)
		panic("stat=%d invalid: must be ATTACHED", ap->stat);

	ap->stat = READING;
	ap->on_read_done = done;
	ap->data_buf = uv_buf_init(buf, len);

	uv_read_start((uv_stream_t *)ap->pipe[0], data_alloc_buffer, data_pipe_read);
}

static void audio_in_stop(audio_in_t *ai) {
	airplay_t *ap = (airplay_t *)ai->in;

	if (ap->stat < STOPPING)
		ap->stat = STOPPING;
	uv_process_kill(ap->proc, 15);
}

static void audio_in_close(audio_in_t *ai, audio_in_close_cb done) {
	airplay_t *ap = (airplay_t *)ai->in;

	if (ap->stat != STOPPING)
		panic("must call after is_eof");

	info("close");

	ap->on_close = done;
	ap->stat = CLOSING_FD1;
	uv_close((uv_handle_t *)ap->pipe[0], on_handle_closed);
}

static int audio_in_is_eof(audio_in_t *ai) {
	airplay_t *ap = (airplay_t *)ai->in;

	return ap->stat > READING;
}

static int audio_in_can_read(audio_in_t *ai) {
	airplay_t *ap = (airplay_t *)ai->in;

	return ap->stat == ATTACHED;
}

void audio_in_airplay_init(uv_loop_t *loop, audio_in_t *ai) {
	if (g_ap == NULL) {
		audio_in_error_init(loop, ai, "please run airplay_start first");
		return;
	}
	if (g_ap->stat < STARTED) {
		audio_in_error_init(loop, ai, "no rstp conn yet");
		return;
	}
	if (g_ap->stat > STARTED) {
		audio_in_error_init(loop, ai, "already attached");
		return;
	}

	g_ap->ai = ai;
	g_ap->stat = ATTACHED;
	info("attached");

	ai->in = g_ap;
	ai->read = audio_in_read;
	ai->stop = audio_in_stop;
	ai->close = audio_in_close;
	ai->can_read = audio_in_can_read;
	ai->is_eof = audio_in_is_eof;
}

