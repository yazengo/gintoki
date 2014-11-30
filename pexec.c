
#include <stdlib.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"

typedef struct {
	uv_process_t *p;
} ctrl_t;

static int ctrl_pid(lua_State *L, uv_loop_t *loop, void *_c) {
	ctrl_t *c = (ctrl_t *)_c;

	if (c->p) {
		lua_pushnumber(L, c->p->pid);
		return 1;
	}

	return 0;
}

static int ctrl_kill(lua_State *L, uv_loop_t *loop, void *_c) {
	ctrl_t *c = (ctrl_t *)_c;
	
	int sig = lua_tonumber(L, 1);
	if (sig == 0)
		sig = 15;

	if (c->p) 
		uv_process_kill(c->p, sig);

	return 0;
}

static ctrl_t *newctrl(lua_State *L, uv_loop_t *loop) {
	ctrl_t *c = (ctrl_t *)luv_newctx(L, loop, sizeof(ctrl_t));

	luv_pushcclosure(L, ctrl_pid, c);
	lua_setfield(L, -2, "pid");

	luv_pushcclosure(L, ctrl_kill, c);
	lua_setfield(L, -2, "kill");

	return c;
}

static void proc_closed(uv_handle_t *h) {
	ctrl_t *c = (ctrl_t *)h->data;
	if (c)
		c->p = NULL;
	free(h);
}

static void proc_on_exit(uv_process_t *p, int stat, int sig) {
	debug("stat=%d sig=%d", stat, sig);

	ctrl_t *c = (ctrl_t *)p->data;
	if (c) {
		lua_State *L = luv_state(c);
		lua_pushnumber(L, stat);
		lua_pushnumber(L, sig);
		luv_callfield(c, "exit_cb", 2, 0);
	}

	uv_close((uv_handle_t *)p, proc_closed);
}

static uv_stdio_container_t 
newpipe(lua_State *L, uv_loop_t *loop, int type) {
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	
	uv_pipe_init(loop, &p->p, 0);
	p->st = (uv_stream_t *)&p->p;
	p->type = type;

	uv_stdio_container_t c = {
		.flags = UV_CREATE_PIPE,
		.data.stream = p->st,
	};

	debug("p=%p", p);

	return c;
}

static void settable(lua_State *L, int i) {
	lua_pushnumber(L, i);
	lua_insert(L, -2);
	lua_settable(L, -3);
}

/*
[stdin, stdout, stderr, c] = pexec('shell commands', 'rwec')
c.exit_cb = function (code)
end
c.pid()
c.kill(9)
*/
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

	ctrl_t *c = NULL;

	lua_newtable(L);

	int n = 0;
	char *i = mode;
	while (i && *i) {
		switch (*i) {
		case 'r':
			stdio[1] = newpipe(L, loop, PSTREAM_SRC);
			n++; settable(L, n);
			break;

		case 'w':
			stdio[0] = newpipe(L, loop, PSTREAM_SINK);
			n++; settable(L, n);
			break;

		case 'e':
			stdio[2] = newpipe(L, loop, PSTREAM_SRC);
			n++; settable(L, n);
			break;

		case 'c':
			c = newctrl(L, loop);
			n++; settable(L, n);
			break;
		}
		i++;
	}

	char *args[] = {"sh", "-c", cmd, NULL};
	opts.file = args[0];
	opts.args = args;
	opts.exit_cb = proc_on_exit;

	uv_process_t *p = (uv_process_t *)zalloc(sizeof(uv_process_t));

	if (c) {
		p->data = c;
		c->p = p;
	}

	int r = uv_spawn(loop, p, opts);
	info("cmd=%s spawn=%d pid=%d", cmd, r, p->pid);

	return 1;
}

void luv_pexec_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pexec", luv_pexec);
}

