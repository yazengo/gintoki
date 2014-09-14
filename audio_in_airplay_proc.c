
#include <uv.h>
#include <lua.h>

#include "utils.h"

typedef struct {
	char ctrl_cmd;
	char data_buf[1024];
} airplay_t;
static airplay_t _ap, *ap = &_ap;

static uv_buf_t ctrl_alloc_buffer(uv_handle_t *h, size_t len) {
	return uv_buf_init(&ap->ctrl_cmd, 1);
}

static void ctrl_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	debug("n=%d", n);
}

static uv_buf_t data_alloc_buffer(uv_handle_t *h, size_t len) {
	return uv_buf_init(&ap->data_buf, sizeof(ap->data_buf));
}

static void data_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	debug("n=%d", n);
}

void luv_airplay_proc_init(char *prog, lua_State *L, uv_loop_t *loop) {
	uv_process_t *proc = (uv_process_t *)zalloc(sizeof(uv_process_t));
	proc->data = av;
	av->proc = proc;

	int i;
	for (i = 0; i < 2; i++) {
		av->pipe[i] = zalloc(sizeof(uv_pipe_t));
		uv_pipe_init(loop, av->pipe[i], 0);
		uv_pipe_open(av->pipe[i], 0);
		av->pipe[i]->data = av;
	}

	uv_stdio_container_t stdio[5] = {
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)av->pipe[0]},
		{.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)av->pipe[1]},
	};
	uv_process_options_t opts = {
		.stdio = stdio,
		.stdio_count = 5,
	};

	char *args[] = {prog, "-test-c", "101", NULL};
	opts.file = args[0];
	opts.args = args;
	opts.exit_cb = proc_on_exit;

	int r = uv_spawn(loop, proc, opts);
	info("spawn=%d pid=%d", r, proc->pid);

	uv_read_start((uv_stream_t *)av->pipe[0], ctrl_alloc_buffer, ctrl_pipe_read);
	uv_read_start((uv_stream_t *)av->pipe[1], data_alloc_buffer, data_pipe_read);
}

