
#include <uv.h>
#include <lua.h>

#include "utils.h"

typedef struct {
	uv_pipe_t *pipe[2];
	char ctrl_cmd;
	char data_buf[1024];
} airplay_t;
static airplay_t _ap, *ap = &_ap;

static uv_buf_t ctrl_alloc_buffer(uv_handle_t *h, size_t len) {
	debug("n=%d", len);
	return uv_buf_init(&ap->ctrl_cmd, 1);
}

static void ctrl_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	debug("n=%d", n);
}

static uv_buf_t data_alloc_buffer(uv_handle_t *h, size_t len) {
	debug("n=%d", len);
	return uv_buf_init(ap->data_buf, sizeof(ap->data_buf));
}

static void data_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	debug("n=%d", n);
}

static void proc_on_exit(uv_process_t *p, int stat, int sig) {
	info("sig=%d", sig);
}

void luv_airplay_proc_init(lua_State *L, uv_loop_t *loop, char *prog) {
	uv_process_t *proc = (uv_process_t *)zalloc(sizeof(uv_process_t));

	int i;
	for (i = 0; i < 2; i++) {
		ap->pipe[i] = (uv_pipe_t *)zalloc(sizeof(uv_pipe_t));
		uv_pipe_init(loop, ap->pipe[i], 0);
		uv_pipe_open(ap->pipe[i], 0);
	}

	uv_stdio_container_t stdio[5] = {
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE|UV_WRITABLE_PIPE, .data.stream = (uv_stream_t *)ap->pipe[0]},
		{.flags = UV_CREATE_PIPE|UV_WRITABLE_PIPE, .data.stream = (uv_stream_t *)ap->pipe[1]},
	};

	char *args[] = {prog, "-test-c", "101", NULL};

	uv_process_options_t opts = {
		.stdio = stdio,
		.stdio_count = 5,
		.file = args[0],
		.args = args,
		.exit_cb = proc_on_exit,
	};

	int r = uv_spawn(loop, proc, opts);
	info("spawn=%d pid=%d", r, proc->pid);

	uv_read_start((uv_stream_t *)ap->pipe[0], ctrl_alloc_buffer, ctrl_pipe_read);
	uv_read_start((uv_stream_t *)ap->pipe[1], data_alloc_buffer, data_pipe_read);
}

