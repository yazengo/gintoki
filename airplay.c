
#include <stdlib.h>

#include <uv.h>
#include <lua.h>

#include "utils.h"
#include "audio_src.h"

/*

audio_src_t *s;

s->name = "airplay://";
s->on_close = ...

audio_src_start(s);
audio_src_write(s, n, done);
audio_src_stop(s);

audio_in_fromsrc_init(s, "airplay://");

*/

typedef struct {
	int stat;

	uv_pipe_t *pipe[2];
	uv_process_t *proc;
	uv_loop_t *loop;

	char ctrl_cmd;
	char data_buf[1024];

	audiosrc_srv_t *as;
} airplay_t;

enum {
	INIT,
	READING,
	STOPPING,
	CLOSING_FD1,
	CLOSING_FD2,
	CLOSING_PROC,
};

static airplay_t *g_ap;

static void proc_start(airplay_t *ap);

static void on_handle_closed(uv_handle_t *h) {
	airplay_t *ap = (airplay_t *)h->data;
	free(h);

	switch (ap->stat) {
	case CLOSING_FD1:
		ap->stat = CLOSING_FD2;
		uv_close((uv_handle_t *)ap->pipe[1], on_handle_closed);
		break;

	case CLOSING_FD2:
		ap->stat = CLOSING_PROC;
		uv_process_kill(ap->proc, 15);
		uv_close((uv_handle_t *)ap->proc, on_handle_closed);
		break;

	case CLOSING_PROC:
		proc_start(ap);
		break;
	}
}

static void on_srv_stopped(audiosrc_srv_t *as) {
	airplay_t *ap = (airplay_t *)as->data;

	free(as);
	ap->as = NULL;
}

static uv_buf_t ctrl_alloc_buffer(uv_handle_t *h, size_t len) {
	airplay_t *ap = (airplay_t *)h->data;

	debug("n=%d", len);

	return uv_buf_init(&ap->ctrl_cmd, 1);
}

static void srv_start(airplay_t *ap) {
	ap->as = (audiosrc_srv_t *)zalloc(sizeof(audiosrc_srv_t));
	ap->as->url = "airplay://";
	audiosrc_srv_start(ap->loop, ap->as);
}

static void srv_stop(airplay_t *ap) {
	audiosrc_srv_stop(ap->as);
	free(ap->as);
	ap->as = NULL;
}

static void ctrl_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	airplay_t *ap = (airplay_t *)st->data;

	debug("n=%d", n);

	if (n <= 0) {
		ap->stat = CLOSING_FD1;
		srv_stop(ap);
		uv_close((uv_handle_t *)st, on_handle_closed);
		return;
	}

	if (ap->ctrl_cmd == 's')
		srv_start(ap);
	if (ap->ctrl_cmd == 'e')
		srv_stop(ap);
}

static uv_buf_t data_alloc_buffer(uv_handle_t *h, size_t len) {
	airplay_t *ap = (airplay_t *)h->data;

	debug("n=%d", len);
	return uv_buf_init(ap->data_buf, sizeof(ap->data_buf));
}

static void data_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	airplay_t *ap = (airplay_t *)st->data;

	uv_read_stop(st);

	int eof = (n < 0);
	if (eof)
		n = 0;

	debug("n=%d", n);

	switch (ap->stat) {
	case READING:
		if (eof) {
			ap->stat = STOPPING;
		} else {
			ap->stat = INIT;
			audiosrc_srv_write(ap->as, buf.base, n);
		}
		break;

	case STOPPING:
		break;

	default:
		panic("must be READING or STOPPING state");
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
	for (i = 0; i < 2; i++) {
		ap->pipe[i] = (uv_pipe_t *)zalloc(sizeof(uv_pipe_t));
		ap->pipe[i]->data = ap;
		uv_pipe_init(ap->loop, ap->pipe[i], 0);
	}

	uv_stdio_container_t stdio[5] = {
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE|UV_WRITABLE_PIPE, .data.stream = (uv_stream_t *)ap->pipe[0]},
		{.flags = UV_CREATE_PIPE|UV_WRITABLE_PIPE, .data.stream = (uv_stream_t *)ap->pipe[1]},
	};

	char *args_test[] = {getenv("_"), "-t", "110", NULL};
	char *args_shairport[] = {"shairport", NULL};
	char **args = args_test;

	uv_process_options_t opts = {
		.stdio = stdio,
		.stdio_count = 5,
		.file = args[0],
		.args = args,
		.exit_cb = proc_on_exit,
	};

	int r = uv_spawn(ap->loop, proc, opts);
	info("spawn=%d pid=%d", r, proc->pid);

	uv_read_start((uv_stream_t *)ap->pipe[0], ctrl_alloc_buffer, ctrl_pipe_read);
	uv_read_start((uv_stream_t *)ap->pipe[1], data_alloc_buffer, data_pipe_read);
}

void luv_airplay_proc_init(lua_State *L, uv_loop_t *loop) {
	g_ap = (airplay_t *)zalloc(sizeof(airplay_t));
	g_ap->loop = loop;
	proc_start(g_ap);
}

