
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include <lua.h>

#include "utils.h"
#include "audio_in.h"

typedef struct airplay_s {
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
	void (*on_killed)(struct airplay_s *ap);
} airplay_t;

enum {
	INIT,
	STARTED,
	KILLING_THEN_RESTART,
	RESTART,

	ATTACHED,
	READING,

	KILLING_WHEN_READING,
	KILLING_WAIT_READING_DONE,
	KILLING_WAIT_EXIT,
	KILLED,

	CLOSING_FD1,
	CLOSING_PROC,
};

static airplay_t *g_ap;

static void proc_start(airplay_t *ap);
static void audio_in_stop(audio_in_t *ai);
static void airplay_stop(airplay_t *ap);
static void airplay_close(airplay_t *ap);

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
		if (ap->on_close)
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
	
	uv_read_stop(st);
	debug("first data comes. n=%d", n);

	if (n <= 0) {
		airplay_stop(ap);
		return;
	}

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
		if (n >= 0) {
			ap->stat = ATTACHED;
			ap->on_read_done(ai, n);
		} else {
			ap->stat = KILLING_WAIT_EXIT;
			uv_process_kill(ap->proc, 15);
			ap->on_read_done(ai, 0);
		}
		break;

	case KILLING_WAIT_READING_DONE:
		ap->stat = KILLED;
		ap->on_read_done(ai, 0);
		break;

	case KILLING_WHEN_READING:
		ap->stat = KILLING_WAIT_EXIT;
		ap->on_read_done(ai, 0);
		break;

	default:
		panic("stat=%d invalid", ap->stat);
	}
}

static void airplay_stop(airplay_t *ap) {
	uv_process_kill(ap->proc, 15);

	switch (ap->stat) {
	case INIT:
	case STARTED:
		ap->stat = KILLING_THEN_RESTART;
		break;

	case ATTACHED:
		ap->stat = KILLING_WAIT_EXIT;
		break;

	case READING:
		ap->stat = KILLING_WHEN_READING;
		break;
	}
}

static void airplay_close(airplay_t *ap) {
	if (ap->stat != KILLED && ap->stat != RESTART)
		panic("stat=%d invalid", ap->stat);
	
	info("close");

	ap->stat = CLOSING_FD1;
	uv_close((uv_handle_t *)ap->pipe[0], on_handle_closed);
}

static void proc_on_killed(uv_process_t *p, int stat, int sig) {
	airplay_t *ap = (airplay_t *)p->data;

	info("sig=%d", sig);

	switch (ap->stat) {
	case KILLING_WAIT_EXIT:
		ap->stat = KILLED;
		break;

	case KILLING_WHEN_READING:
		ap->stat = KILLING_WAIT_READING_DONE;
		break;

	case INIT:
	case STARTED:
	case KILLING_THEN_RESTART:
		ap->stat = RESTART;
		airplay_close(ap);
		break;

	case ATTACHED:
		ap->stat = KILLED;
		break;

	case READING:
		ap->stat = KILLING_WAIT_READING_DONE;
		break;

	default:
		panic("stat=%d invalid", ap->stat);
	}

	if (ap->on_killed)
		ap->on_killed(ap);
}

static void proc_start(airplay_t *ap) {
	uv_process_t *proc = (uv_process_t *)zalloc(sizeof(uv_process_t));
	proc->data = ap;
	ap->proc = proc;

	ap->on_close = NULL;
	ap->on_killed = NULL;

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
		.exit_cb = proc_on_killed,
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
		info("do restart. stat=%d", g_ap->stat);

		free(g_ap->name);
		g_ap->name = name;

		airplay_stop(g_ap);
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

	debug("n=%d", len);

	ap->stat = READING;
	ap->on_read_done = done;
	ap->data_buf = uv_buf_init(buf, len);

	uv_read_start((uv_stream_t *)ap->pipe[0], data_alloc_buffer, data_pipe_read);
}

static void audio_in_stop(audio_in_t *ai) {
	airplay_t *ap = (airplay_t *)ai->in;

	airplay_stop(ap);
}

static void audio_in_close(audio_in_t *ai, audio_in_close_cb done) {
	airplay_t *ap = (airplay_t *)ai->in;

	debug("stat=%d", ap->stat);
	ap->on_close = done;

	airplay_close(ap);
}

static int audio_in_is_eof(audio_in_t *ai) {
	airplay_t *ap = (airplay_t *)ai->in;

	debug("stat=%d", ap->stat);
	return ap->stat == KILLED;
}

static int audio_in_can_read(audio_in_t *ai) {
	airplay_t *ap = (airplay_t *)ai->in;

	debug("stat=%d", ap->stat);
	return ap->stat == ATTACHED;
}

void audio_in_airplay_init(uv_loop_t *loop, audio_in_t *ai) {
	if (g_ap == NULL) {
		warn("please run airplay_start first");
		audio_in_error_init(loop, ai, "please run airplay_start first");
		return;
	}
	if (g_ap->stat < STARTED) {
		warn("no rstp conn yet");
		audio_in_error_init(loop, ai, "no rstp conn yet");
		return;
	}
	if (g_ap->stat > STARTED) {
		warn("already attached");
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

