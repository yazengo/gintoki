
// please visit http://www.processon.com/view/link/542c8e090cf2e6eabf18d01d

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
	CMD_END,
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
	void (*on_close)(struct airplay_s *);

	unsigned pending_killed:1;

	char *name;

	audio_in_t *ai;

	uv_pipe_t *pipe[1];
	uv_process_t *proc;
	int pid;

	lua_State *L;
	uv_loop_t *loop;

	readbuf_t rb;

	audio_in_read_cb ai_read_done;
	audio_in_close_cb ai_close_done;
} airplay_t;

enum {
	PDATA,
};

enum {
	READING_FIRST_START,
	READING_FIRST_DATA,
	STARTED,

	ATTACHED_READ_DATA,
	ATTACHED_READ_CMD,

	READING_DATA,
	READING_CMD,

	ENDED,
	ENDED_CLOSING,

	RESTARTING_WAIT_READ,
	STOPPING_WAIT_READ,
	STOPPING_WAIT_AI_READ,
	STOPPING_WAIT_AI_READING,
	STOPPED,

	READ_EOF,
	CLOSING,
};

enum {
	CLOSING_PDATA,
	CLOSING_WAITING_EXIT,
	CLOSING_PROC,
};

static airplay_t *g_ap;

static void proc_init(airplay_t *ap);
static void proc_start(airplay_t *ap);
static void proc_kill(airplay_t *ap);
static void proc_close(airplay_t *ap);

static void enter_restart(airplay_t *ap);
static void enter_stopped(airplay_t *ap);
static void enter_closing(airplay_t *ap, void (*done)(airplay_t *));

static void pdata_read_cmdhdr(airplay_t *ap);
static void pdata_read_data(airplay_t *ap, void *buf, int len);
static void pdata_read_start(airplay_t *ap);

static void on_read_eof(airplay_t *ap);
static void lua_emit_start(airplay_t *ap);

static void on_handle_closed(uv_handle_t *h) {
	airplay_t *ap = (airplay_t *)h->data;
	free(h);

	debug("stat=%d stat_close=%d", ap->stat, ap->stat_close);

	switch (ap->stat_close) {
	case CLOSING_PDATA:
		if (ap->pending_killed) {
			proc_close(ap);
			debug("do close");
		} else {
			ap->stat_close = CLOSING_WAITING_EXIT;
			debug("do kill");
			proc_kill(ap);
		}
		break;

	case CLOSING_PROC:
		ap->on_close(ap);
		break;
	}
}

static void on_cmd_start(airplay_t *ap) {
	info("stat=%d", ap->stat);

	switch (ap->stat) {
	case READING_FIRST_START:
		ap->stat = READING_FIRST_DATA;
		pdata_read_cmdhdr(ap);
		break;

	default:
		panic("invalid stat=%d", ap->stat);
	}
}

static void on_cmd_end(airplay_t *ap) {
	info("stat=%d", ap->stat);

	switch (ap->stat) {
	case READING_CMD:
		ap->stat = ENDED;
		ap->ai_read_done(ap->ai, -1);
		break;

	default:
		panic("invalid stat=%d", ap->stat);
	}
}

static void on_cmd_data(airplay_t *ap) {
	debug("stat=%d", ap->stat);

	switch (ap->stat) {
	case READING_CMD:
		ap->stat = ATTACHED_READ_DATA;
		ap->ai_read_done(ap->ai, 0);
		break;

	case READING_FIRST_DATA:
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
	case READING_DATA:
		ap->stat = ATTACHED_READ_CMD;
		ap->ai_read_done(ap->ai, len);
		break;

	default:
		panic("invalid stat=%d", ap->stat);
	}
}

static void on_data_halfdone(airplay_t *ap, int len) {
	debug("stat=%d", ap->stat);

	switch (ap->stat) {
	case READING_DATA:
		ap->stat = ATTACHED_READ_DATA;
		ap->ai_read_done(ap->ai, len);
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

	case CMD_END:
		on_cmd_end(ap);
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

	switch (ap->stat) {
	case RESTARTING_WAIT_READ:
		enter_closing(ap, enter_restart);
		return;
	
	case STOPPING_WAIT_READ:
		enter_stopped(ap);
		return;
	}

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

static void enter_restart(airplay_t *ap) {
	debug("restart");
	proc_init(ap);
	proc_start(ap);
}

static void enter_stopped(airplay_t *ap) {
	ap->stat = STOPPED;
	ap->ai_read_done(ap->ai, -1);
}

static void enter_close_then_start(airplay_t *ap) {
	ap->ai_close_done(ap->ai);
	proc_start(ap);
}

static void enter_close_then_restart(airplay_t *ap) {
	ap->ai_close_done(ap->ai);
	proc_init(ap);
	proc_start(ap);
}

static void enter_closing(airplay_t *ap, void (*done)(airplay_t *)) {
	ap->stat = CLOSING;
	ap->on_close = done;
	ap->stat_close = CLOSING_PDATA;
	uv_close((uv_handle_t *)ap->pipe[PDATA], on_handle_closed);
}

typedef struct {
	uv_call_t c;
	void (*done)(airplay_t *ap);
	airplay_t *ap;
} ai_closing_t;

static void on_ai_closing_done(uv_call_t *c) {
	ai_closing_t *ac = (ai_closing_t *)c->data;
	airplay_t *ap = ac->ap;

	debug("done");
	ac->done(ap);

	free(ac);
}

static void enter_ai_closing(airplay_t *ap, int stat, void (*done)(airplay_t *ap)) {
	debug("close");

	ai_closing_t *ac = (ai_closing_t *)zalloc(sizeof(ai_closing_t));
	ac->ap = ap;
	ac->done = done;
	ac->c.data = ac;
	ac->c.done_cb = on_ai_closing_done;

	ap->stat = stat;
	uv_call(ap->loop, &ac->c);
}

static void on_read_eof(airplay_t *ap) {
	debug("stat=%d", ap->stat);

	switch (ap->stat) {
	case READING_FIRST_START:
	case READING_FIRST_DATA:
		enter_closing(ap, enter_restart);
		break;

	case READING_DATA:
	case READING_CMD:
		ap->stat = READ_EOF;
		ap->ai_read_done(ap->ai, -1);
		break;

	default:
		panic("invalid stat=%d", ap->stat);
	}
}

static void on_proc_exit(uv_process_t *p, int stat, int sig) {
	airplay_t *ap = (airplay_t *)p->data;

	info("sig=%d stat=%d stat_close=%d", sig, ap->stat, ap->stat_close);

	if (ap->stat == CLOSING && ap->stat_close == CLOSING_WAITING_EXIT)
		proc_close(ap);
	else
		ap->pending_killed++;
}

static void proc_init(airplay_t *ap) {
	uv_process_t *proc = (uv_process_t *)zalloc(sizeof(uv_process_t));
	proc->data = ap;
	ap->proc = proc;

	ap->ai_close_done = NULL;
	ap->ai_read_done = NULL;
	ap->pending_killed = 0;
	ap->on_close = NULL;
	ap->stat_close = 0;

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
		*a++ = "pipe2";

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
	ap->pid = proc->pid;
}

static void proc_start(airplay_t *ap) {
	ap->stat = READING_FIRST_START;
	pdata_read_cmdhdr(ap);
}

static void proc_close(airplay_t *ap) {
	debug("stat=%d", ap->stat);
	ap->stat_close = CLOSING_PROC;
	uv_close((uv_handle_t *)ap->proc, on_handle_closed);
}

static void proc_kill(airplay_t *ap) {
	info("pid=%d stat=%d", ap->proc->pid, ap->stat);
	uv_process_kill(ap->proc, 15);
}

static void proc_restart(airplay_t *ap) {
	info("stat=%d", ap->stat);

	switch (ap->stat) {
	case READING_FIRST_START:
	case READING_FIRST_DATA:
		ap->stat = RESTARTING_WAIT_READ;
		proc_kill(ap);
		break;

	case STARTED:
		enter_closing(ap, enter_restart);
		break;

	case READING_DATA:
	case READING_CMD:
		ap->stat = STOPPING_WAIT_READ;
		proc_kill(ap);
		break;

	case ATTACHED_READ_DATA:
	case ATTACHED_READ_CMD:
		ap->stat = STOPPING_WAIT_AI_READ;
		break;
	}
}


static void audio_in_read(audio_in_t *ai, void *buf, int len, audio_in_read_cb done) {
	airplay_t *ap = (airplay_t *)ai->in;

	debug("stat=%d len=%d", ap->stat, len);

	int left = ap->rb.len - ap->rb.got;
	if (len > left)
		len = left;

	switch (ap->stat) {
	case ATTACHED_READ_DATA:
		ap->stat = READING_DATA;
		pdata_read_data(ap, buf, len);
		break;

	case ATTACHED_READ_CMD:
		ap->stat = READING_CMD;
		pdata_read_cmdhdr(ap);
		break;

	case STOPPING_WAIT_AI_READ:
		enter_ai_closing(ap, STOPPING_WAIT_AI_READING, enter_stopped);
		break;

	default:
		panic("stat=%d invalid", ap->stat);
	}

	ap->ai_read_done = done;
}

static void audio_in_stop_read(audio_in_t *ai) {
	airplay_t *ap = (airplay_t *)ai->in;

	proc_restart(ap);
}

static void audio_in_close(audio_in_t *ai, audio_in_close_cb done) {
	airplay_t *ap = (airplay_t *)ai->in;

	debug("stat=%d", ap->stat);
	ap->ai_close_done = done;

	switch (ap->stat) {
	case ATTACHED_READ_DATA:
	case ATTACHED_READ_CMD:
	case STOPPED:
		enter_closing(ap, enter_close_then_restart);
		break;
		
	case ENDED:
		enter_ai_closing(ap, ENDED_CLOSING, enter_close_then_start);
		break;

	case READ_EOF:
		enter_closing(ap, enter_close_then_restart);
		break;

	default:
		panic("stat=%d invalid", ap->stat);
	}
}

void audio_in_airplay_v2_init(uv_loop_t *loop, audio_in_t *ai) {
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
	g_ap->stat = ATTACHED_READ_DATA;
	info("attached");

	ai->in = g_ap;
	ai->read = audio_in_read;
	ai->stop_read = audio_in_stop_read;
	ai->close = audio_in_close;
}

static void lua_emit_start(airplay_t *ap) {
	lua_getglobal(ap->L, "airplay_on_start");
	if (!lua_isnil(ap->L, -1)) {
		lua_call_or_die(ap->L, 0, 0);
	} else 
		lua_pop(ap->L, 1);
}

// arg[1] = name
static int lua_airplay_start(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));
	char *name = (char *)lua_tostring(L, 1);

	if (name == NULL)
		panic("name must be set");
	name = strdup(name);

	if (g_ap) {
		info("do restart. stat=%d", g_ap->stat);

		free(g_ap->name);
		g_ap->name = name;

		proc_restart(g_ap);
		return 0;
	}

	g_ap = (airplay_t *)zalloc(sizeof(airplay_t));
	g_ap->L = L;
	g_ap->loop = loop;
	g_ap->name = name;

	proc_init(g_ap);
	proc_start(g_ap);

	return 0;
}

static void onexit() {
	if (g_ap && g_ap->pid) {
		int r;
		kill(g_ap->pid, SIGTERM);
		waitpid(g_ap->pid, &r, 0);
	}
}

void luv_airplay_v2_init(lua_State *L, uv_loop_t *loop) {
	// airplay_start = [native function]
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_airplay_start, 1);
	lua_setglobal(L, "airplay_start");

	utils_onexit(onexit);
}

