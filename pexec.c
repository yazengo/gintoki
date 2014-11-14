
#include "luv.h"
#include "utils.h"
#include "pipe.h"

static void proc_on_exit(uv_process_t *p, int stat, int sig) {
	uv_close((uv_handle_t *)p, NULL);
}

static int luv_pexec(lua_State *L, uv_loop_t *loop) {
	char *cmd = (char *)lua_tostring(L, 1);

	if (cmd == NULL) 
		panic("cmd must be set");

	lua_getfield(L, 2, "stdin");
	int need_stdin = lua_toboolean(L, -1);

	lua_getfield(L, 2, "stdout");
	int need_stdout = lua_toboolean(L, -1);

	lua_getfield(L, 2, "stderr");
	int need_stderr = lua_toboolean(L, -1);

	lua_newtable(L);
	int t = lua_gettop(L);

	uv_process_options_t opts = {};

	uv_stdio_container_t stdio[3] = {
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
	};
	opts.stdio = stdio;
	opts.stdio_count = 3;

	if (need_stdin) {
		pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
		uv_stdio_container_t c = {
			.flags = UV_CREATE_PIPE,
			.data.stream = (uv_stream_t *)&p->p,
		};
		stdio[0] = c;
		lua_setfield(L, t, "stdin");
	}

	if (need_stdout) {
		pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
		uv_stdio_container_t c = {
			.flags = UV_CREATE_PIPE,
			.data.stream = (uv_stream_t *)&p->p,
		};
		stdio[1] = c;
		lua_setfield(L, t, "stdout");
	}

	if (need_stderr) {
		pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
		uv_stdio_container_t c = {
			.flags = UV_CREATE_PIPE,
			.data.stream = (uv_stream_t *)&p->p,
		};
		stdio[2] = c;
		lua_setfield(L, t, "stderr");
	}

	char *args[] = {"sh", "-c", cmd, NULL};
	opts.file = args[0];
	opts.args = args;
	opts.exit_cb = proc_on_exit;

	uv_process_t *proc = (uv_process_t *)zalloc(sizeof(uv_process_t));

	int r = uv_spawn(loop, proc, opts);
	info("cmd=%s spawn=%d pid=%d", cmd, r, proc->pid);
}

void luv_pexec_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pexec", luv_pexec);
}

