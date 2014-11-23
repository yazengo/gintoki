
#include <stdlib.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"

static void proc_closed(uv_handle_t *h) {
	free(h);
}

static void proc_on_exit(uv_process_t *p, int stat, int sig) {
	debug("r=%d", stat);
	uv_close((uv_handle_t *)p, proc_closed);
}

static uv_stdio_container_t 
newpipe(lua_State *L, uv_loop_t *loop, int type) {
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	p->type = type;

	uv_pipe_init(loop, &p->p, 0);
	p->st = (uv_stream_t *)&p->p;

	uv_stdio_container_t c = {
		.flags = UV_CREATE_PIPE,
		.data.stream = p->st,
	};
	return c;
}

// stdout = pexec('ls -l', 'r')
// stdin, stdout, stderr = pexec('cat >l', 'rwe')
// stdin, stderr = pexec('curl ...', 'we')
static int luv_pexec(lua_State *L, uv_loop_t *loop) {
	char *cmd = (char *)lua_tostring(L, 1);
	char *mode = (char *)lua_tostring(L, 2);

	if (cmd == NULL)
		panic("cmd must be set");

	uv_process_options_t opts = {};

	uv_stdio_container_t stdio[3] = {
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
		{.flags = UV_IGNORE},
	};
	opts.stdio = stdio;
	opts.stdio_count = 3;

	int n = 0;
	char *i = mode;
	while (i && *i) {
		switch (*i) {
		case 'r':
			stdio[1] = newpipe(L, loop, PSTREAM_SRC);
			n++;
			break;
		case 'w':
			stdio[0] = newpipe(L, loop, PSTREAM_SINK);
			n++;
			break;
		case 'e':
			stdio[2] = newpipe(L, loop, PSTREAM_SRC);
			n++;
			break;
		}
		i++;
	}

	char *args[] = {"sh", "-c", cmd, NULL};
	opts.file = args[0];
	opts.args = args;
	opts.exit_cb = proc_on_exit;

	uv_process_t *proc = (uv_process_t *)zalloc(sizeof(uv_process_t));

	int r = uv_spawn(loop, proc, opts);
	info("cmd=%s spawn=%d pid=%d", cmd, r, proc->pid);

	return n;
}

void luv_pexec_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pexec", luv_pexec);
}

