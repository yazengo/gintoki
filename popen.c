
#include <uv.h>
#include <lua.h>

#include "pipe.h"
#include "utils.h"
#include "strbuf.h"

static void proc_on_closed(uv_handle_t *h) {
	free(h);
}

static void proc_on_exit(uv_process_t *proc, int stat, int sig) {
	uv_close((uv_handle_t *)proc, proc_on_closed);
}

// popen('ls -l /tmp')
static int luv_popen(lua_State *L, uv_loop_t *loop) {
	char *cmd = (char *)lua_tostring(L, 1);
	if (cmd == NULL)
		panic("cmd must be set");

	uv_pipe_t *pstdio[2];
	int i;
	for (i = 0; i < 2; i++) {
		pstdio[i] = (uv_pipe_t *)zalloc(sizeof(uv_pipe_t));
		uv_pipe_init(loop, pstdio[i], i);
	}

	uv_process_options_t opts = {};
	uv_stdio_container_t stdio[2] = {
		{.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)pstdio[0]},
		{.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)pstdio[1]},
	};
	opts.stdio = stdio;
	opts.stdio_count = 2;

	char *args[] = {"sh", "-c", cmd, NULL};
	opts.file = args[0];
	opts.args = args;
	opts.exit_cb = proc_on_exit;

	uv_process_t *proc = (uv_process_t *)zalloc(sizeof(uv_process_t));

	int r = uv_spawn(loop, proc, opts);
	info("cmd=%s spawn=%d pid=%d", cmd, r, proc->pid);

	pipe_t *p = luv_newpipe(L, loop);
	p->w = pstdio[0];
	p->r = pstdio[1];

	return 1;
}

// p.close_write()
// p.close_read()
void luv_popen_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "popen", luv_popen);
}

