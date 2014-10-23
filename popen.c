
#include <uv.h>
#include <lua.h>

#include "utils.h"
#include "strbuf.h"

typedef struct {
	uv_process_t proc;
	lua_State *L;
	strbuf_t *sb;
	char buf[2048];
	int code;
	int stat;
	uv_pipe_t pipe[1];
} popen_t;

enum {
	PSTDOUT,
};

enum {
	INIT,
	WAITING_PROC_EXIT,
	WAITING_STDOUT_CLOSE,
};

static void popen_done(popen_t *p) {
	lua_pushlstring(p->L, p->sb->buf, p->sb->length);
	lua_pushnumber(p->L, p->code);
	lua_do_global_callback(p->L, "popen", p, 2, 1);
	strbuf_free(p->sb);
}

static void pstdout_on_closed(uv_handle_t *h) {
	popen_t *p = (popen_t *)h->data;

	switch (p->stat) {
	case INIT:
		p->stat = WAITING_PROC_EXIT;
		break;

	case WAITING_STDOUT_CLOSE:
		popen_done(p);
		break;

	default:
		panic("stat=%d invalid", p->stat);
	}
}

static void proc_on_closed(uv_handle_t *h) {
	popen_t *p = (popen_t *)h->data;

	switch (p->stat) {
	case INIT:
		p->stat = WAITING_STDOUT_CLOSE;
		break;

	case WAITING_PROC_EXIT:
		popen_done(p);
		break;
		
	default: 
		panic("stat=%d invalid", p->stat);
	}
}

static uv_buf_t pstdout_alloc_buffer(uv_handle_t *h, size_t len) {
	popen_t *p = (popen_t *)h->data;

	return uv_buf_init(p->buf, sizeof(p->buf));
}

static void pstdout_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	popen_t *p = (popen_t *)st->data;

	if (n < 0) {
		uv_close((uv_handle_t *)st, pstdout_on_closed);
		return;
	}

	strbuf_append_mem(p->sb, buf.base, n);
}

static void proc_on_exit(uv_process_t *proc, int stat, int sig) {
	popen_t *p = (popen_t *)proc->data;
	p->code = stat;
	uv_close((uv_handle_t *)proc, proc_on_closed);
}

// {
//   cmd = 'ls -l /tmp',
//   done = function (r, code)
//   end,
// }
static int lua_popen(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	lua_getfield(L, 1, "cmd");
	char *cmd = (char *)lua_tostring(L, -1);
	if (cmd == NULL)
		panic("cmd must be set");

	popen_t *p = (popen_t *)zalloc(sizeof(popen_t));
	p->L = L;
	p->proc.data = p;
	p->sb = strbuf_new(512);

	uv_pipe_init(loop, &p->pipe[PSTDOUT], 0);
	uv_pipe_open(&p->pipe[PSTDOUT], 0);
	p->pipe[PSTDOUT].data = p;

	uv_process_options_t opts = {};

	uv_stdio_container_t stdio[2] = {
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)&p->pipe[PSTDOUT]},
	};
	opts.stdio = stdio;
	opts.stdio_count = 2;

	char *args[] = {"sh", "-c", cmd, NULL};
	opts.file = args[0];
	opts.args = args;
	opts.exit_cb = proc_on_exit;

	lua_getfield(L, 1, "done");
	lua_set_global_callback(L, "popen", p);

	int r = uv_spawn(loop, &p->proc, opts);
	info("cmd=%s spawn=%d pid=%d", cmd, r, p->proc.pid);

	uv_read_start((uv_stream_t *)&p->pipe[PSTDOUT], pstdout_alloc_buffer, pstdout_pipe_read);

	return 0;
}

void luv_popen_init(lua_State *L, uv_loop_t *loop) {
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_popen, 1);
	lua_setglobal(L, "popen");
}

