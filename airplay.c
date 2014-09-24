
#include <stdlib.h>
#include <string.h>

#include <uv.h>
#include <lua.h>

#include "utils.h"
#include "audio_in.h"

typedef struct {
	int type;
	int len;
} cmdhdr_t;

enum {
	CMD_START,
	CMD_DATA,
	CMD_STOP,
};

typedef struct {
	int stat;
	int len;
	int got;
	cmdhdr_t cmd;
	uv_buf_t data;
	int data_got;
} readbuf_t;

enum {
	RB_CMD,
	RB_DATA,
};

typedef struct airplay_s {
	int stat;
	int stat_close;

	int stat_kill;
	void (*on_killed)(struct airplay_s *);
	int stopped_cancelled;

	char *name;

	audio_in_t *ai;

	uv_pipe_t *pipe[2];
	uv_process_t *proc;

	lua_State *L;
	uv_loop_t *loop;

	readbuf_t rb;

	audio_in_read_cb on_read_done;
	audio_in_close_cb on_close;
} airplay_t;

enum {
	PDATA,
	PCTRL,
};

enum {
	WAITING_START_CMDHDR,
	WAITING_START_DATAHDR,
	STARTED,
	ATTACHED,
	WAITING_DATA,
	WAITING_CMDHDR,
	STOPPED,
	STOPPED_CLOSING,

	KILLING,
	KILLED,
	CLOSING,
};

enum {
	KILLING_WAIT_EXIT_EOF,
	KILLING_WAIT_EOF,
	KILLING_WAIT_EXIT,
};

enum {
	CLOSING_PDATA,
	CLOSING_PROC,
};

static airplay_t *g_ap;

static void proc_start(airplay_t *ap);

static void airplay_stop(airplay_t *ap);
static void airplay_close(airplay_t *ap);

static void pdata_read_cmdhdr(airplay_t *ap);
static void pdata_read_data(airplay_t *ap, void *buf, int len);
static void pdata_read_start(airplay_t *ap);

static void stopped_cancel_closing(airplay_t *ap);

static void on_read_eof(airplay_t *ap);

static void on_handle_closed(uv_handle_t *h) {
	airplay_t *ap = (airplay_t *)h->data;
	free(h);

	debug("stat_close=%d", ap->stat_close);

	switch (ap->stat_close) {
	case CLOSING_PDATA:
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

static void lua_emit_start(airplay_t *ap) {
	lua_getglobal(ap->L, "airplay_on_start");
	if (!lua_isnil(ap->L, -1)) {
		lua_call_or_die(ap->L, 0, 0);
	} else 
		lua_pop(ap->L, 1);
}

static void on_cmd_start(airplay_t *ap) {
	info("stat=%d", ap->stat);

	switch (ap->stat) {
	case WAITING_START_CMDHDR:
		ap->stat = WAITING_START_DATAHDR;
		pdata_read_cmdhdr(ap);
		break;

	default:
		panic("invalid stat=%d", ap->stat);
	}
}

static void on_cmd_stop(airplay_t *ap) {
	info("stat=%d", ap->stat);

	switch (ap->stat) {
	case WAITING_CMDHDR:
		ap->stat = STOPPED;
		break;

	default:
		panic("invalid stat=%d", ap->stat);
	}
}

static void on_cmd_data(airplay_t *ap) {
	debug("stat=%d", ap->stat);

	switch (ap->stat) {
	case WAITING_CMDHDR:
		ap->stat = ATTACHED;
		break;

	case WAITING_START_DATAHDR:
		ap->stat = STARTED;
		lua_emit_start(ap);
		break;

	default:
		panic("invalid stat=%d", ap->stat);
	}
}

static void on_data_done(airplay_t *ap, int len) {
	debug("stat=%d", ap->stat);

	switch (ap->stat) {
	case WAITING_DATA:
		ap->stat = WAITING_CMDHDR;
		pdata_read_cmdhdr(ap);
		ap->on_read_done(ap->ai, len);
		ap->on_read_done = NULL;
		break;

	default:
		panic("invalid stat=%d", ap->stat);
	}
}

static void on_data_halfdone(airplay_t *ap, int len) {
	debug("stat=%d", ap->stat);

	switch (ap->stat) {
	case WAITING_DATA:
		ap->stat = ATTACHED;
		ap->on_read_done(ap->ai, len);
		ap->on_read_done = NULL;
		break;

	default:
		panic("invalid stat=%d", ap->stat);
	}
}

static void pdata_on_got_cmdhdr(airplay_t *ap) {
	readbuf_t *rb = &ap->rb;

	debug("type=%d", rb->cmd.type);

	switch (rb->cmd.type) {
	case CMD_START:
		on_cmd_start(ap);
		break;

	case CMD_STOP:
		on_cmd_stop(ap);
		break;

	case CMD_DATA:
		rb->stat = RB_DATA;
		rb->got = 0;
		rb->len = rb->cmd.len;
		on_cmd_data(ap);
		break;
	}
}

static void pdata_on_read_done(airplay_t *ap, ssize_t n, uv_buf_t buf) {
	audio_in_t *ai = ap->ai;
	readbuf_t *rb = &ap->rb;

	rb->got += n;
	debug("n=%d %d/%d", n, rb->got, rb->len);

	if (rb->stat == RB_DATA)
		rb->data_got += n;

	if (rb->got == rb->len) {
		switch (rb->stat) {
		case RB_CMD:
			pdata_on_got_cmdhdr(ap);
			break;
		case RB_DATA:
			on_data_done(ap, rb->data_got);
			break;
		}
	} else {
		switch (rb->stat) {
		case RB_CMD:
			pdata_read_start(ap);
			break;
		case RB_DATA:
			on_data_halfdone(ap, rb->data_got);
			break;
		}
	}
}

static uv_buf_t pdata_alloc_buffer(uv_handle_t *h, size_t len) {
	airplay_t *ap = (airplay_t *)h->data;
	readbuf_t *rb = &ap->rb;
	uv_buf_t r = {};

	switch (rb->stat) {
	case RB_CMD:
		r.base = ((void *)&rb->cmd) + rb->got;
		r.len = rb->len - rb->got;
		break;

	case RB_DATA:
		r.base = rb->data.base + rb->data_got;
		r.len = rb->data.len - rb->data_got;
		break;
	}

	debug("rb.stat=%d rb=%d/%d buf.len=%d", rb->stat, rb->got, rb->len, r.len);

	return r;
}

static void pdata_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	airplay_t *ap = (airplay_t *)st->data;
	audio_in_t *ai = ap->ai;

	debug("n=%d stat=%d", n, ap->stat);
	uv_read_stop(st);

	if (n < 0) {
		on_read_eof(ap);
		return;
	}

	pdata_on_read_done(ap, n, buf);
}

static void pdata_read_start(airplay_t *ap) {
	uv_read_start((uv_stream_t *)ap->pipe[PDATA], pdata_alloc_buffer, pdata_pipe_read);
}

static void pdata_read_cmdhdr(airplay_t *ap) {
	readbuf_t *rb = &ap->rb;

	rb->stat = RB_CMD;
	rb->got = 0;
	rb->len = sizeof(rb->cmd);

	pdata_read_start(ap);
}

static void pdata_read_data(airplay_t *ap, void *buf, int len) {
	readbuf_t *rb = &ap->rb;

	if (rb->stat != RB_DATA)
		panic("invalid rb.stat=%d", rb->stat);

	if (len > rb->len - rb->got)
		len = rb->len - rb->got;

	rb->data = uv_buf_init(buf, len);
	rb->data_got = 0;

	pdata_read_start(ap);
}

static void jump_to_closing(airplay_t *ap) {
	ap->stat = CLOSING;
	ap->stat_close = CLOSING_PDATA;
	uv_close((uv_handle_t *)ap->pipe[PDATA], on_handle_closed);
}

static void jump_to_killed(airplay_t *ap) {
	ap->stat = KILLED;
}

static void proc_kill(airplay_t *ap) {
	uv_process_kill(ap->proc, 15);
}

static void enter_killing(airplay_t *ap, int stat, void (*done)(airplay_t *)) {
	proc_kill(ap);
	ap->on_killed = done;
	ap->stat_kill = stat;
	ap->stat = KILLING;
}

static void on_read_eof(airplay_t *ap) {
	debug("stat=%d", ap->stat);

	switch (ap->stat) {
	case WAITING_START_CMDHDR:
	case WAITING_START_DATAHDR:
		enter_killing(ap, KILLING_WAIT_EXIT, jump_to_closing);
		break;

	case WAITING_DATA:
	case WAITING_CMDHDR:
		enter_killing(ap, KILLING_WAIT_EXIT, jump_to_killed);
		break;

	default:
		panic("invalid stat=%d", ap->stat);
	}
}

static void on_proc_exit_killing(airplay_t *ap) {
	switch (ap->stat_kill) {
	case KILLING_WAIT_EXIT_EOF:
		ap->stat_kill = KILLING_WAIT_EOF;
		break;

	case KILLING_WAIT_EXIT:
		ap->on_killed(ap);
		break;

	default:
		panic("invalid stat_kill=%d", ap->stat_kill);
	}
}

static void on_proc_exit(uv_process_t *p, int stat, int sig) {
	airplay_t *ap = (airplay_t *)p->data;

	info("sig=%d stat_kill=%d", sig, ap->stat_kill);

	switch (ap->stat) {
	case KILLING:
		on_proc_exit_killing(ap);
		break;

	case WAITING_START_CMDHDR:
	case WAITING_START_DATAHDR:
		enter_killing(ap, KILLING_WAIT_EOF, jump_to_closing);
		break;

	case STARTED:
		jump_to_closing(ap);
		break;

	case WAITING_DATA:
	case WAITING_CMDHDR:
		enter_killing(ap, KILLING_WAIT_EOF, jump_to_killed);
		break;

	case ATTACHED:
	case STOPPED:
		jump_to_killed(ap);
		break;

	case STOPPED_CLOSING:
		stopped_cancel_closing(ap);
		jump_to_closing(ap);
		break;

	default:
		panic("invalid stat=%d", ap->stat);
	}
}

static void on_stopped_close_done(uv_call_t *c) {
	airplay_t *ap = (airplay_t *)c->data;
	free(c);

	debug("done");

	if (ap->stopped_cancelled)
		return;

	ap->stat = WAITING_START_CMDHDR;
	pdata_read_cmdhdr(ap);
}

static void stopped_close(airplay_t *ap) {
	uv_call_t *c = (uv_call_t *)zalloc(sizeof(uv_call_t));
	c->data = ap;
	c->done_cb = on_stopped_close_done;
	ap->stopped_cancelled = 0;
	ap->stat = STOPPED_CLOSING;
	uv_call(ap->loop, c);
}

static void stopped_cancel_closing(airplay_t *ap) {
	ap->stopped_cancelled = 1;
}

static void airplay_close(airplay_t *ap) {
	info("stat=%d", ap->stat);

	switch (ap->stat) {
	case STOPPED:
		stopped_close(ap);
		break;

	case KILLED:
		jump_to_closing(ap);
		break;

	default:
		panic("stat=%d invalid", ap->stat);
	}
}

static void proc_start(airplay_t *ap) {
	uv_process_t *proc = (uv_process_t *)zalloc(sizeof(uv_process_t));
	proc->data = ap;
	ap->proc = proc;

	ap->on_close = NULL;
	ap->on_read_done = NULL;

	int i;
	for (i = 0; i < 2; i++) {
		ap->pipe[i] = (uv_pipe_t *)zalloc(sizeof(uv_pipe_t));
		ap->pipe[i]->data = ap;
		uv_pipe_init(ap->loop, ap->pipe[i], 0);
	}

	uv_stdio_container_t stdio[5] = {
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE|UV_WRITABLE_PIPE, .data.stream = (uv_stream_t *)ap->pipe[PCTRL]},
		{.flags = UV_CREATE_PIPE|UV_WRITABLE_PIPE, .data.stream = (uv_stream_t *)ap->pipe[PDATA]},
	};

	char *args[16] = {};
	char **a = args;

	if (getenv("AIRPLAY_TEST")) {
		info("running test");
		*a++ = getenv("_");
		*a++ = "-t";
		*a++ = "110";
	} else {
		*a++ = "shairport";
		*a++ = "-a";
		*a++ = ap->name;
		*a++ = "-o";
		*a++ = "pipe";

		char *s = getenv("AIRPLAY_LOG");
		if (s) {
			*a++ = "-l";
			*a++ = s;
		}
	}

	uv_process_options_t opts = {
		.stdio = stdio,
		.stdio_count = 5,
		.file = args[0],
		.args = args,
		.exit_cb = on_proc_exit,
	};

	int r = uv_spawn(ap->loop, proc, opts);
	info("proc=%s spawn=%d pid=%d", args[0], r, proc->pid);

	ap->stat = WAITING_START_CMDHDR;
	pdata_read_cmdhdr(ap);
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

		proc_kill(g_ap);
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

	int left = ap->rb.len - ap->rb.got;
	if (len > left)
		len = left;

	ap->stat = WAITING_DATA;
	ap->on_read_done = done;
	pdata_read_data(ap, buf, len);
}

static void audio_in_stop(audio_in_t *ai) {
	airplay_t *ap = (airplay_t *)ai->in;

	proc_kill(ap);
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
	return ap->stat == KILLED || ap->stat == STOPPED;
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

