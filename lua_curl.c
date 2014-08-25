
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>

#include "strbuf.h"
#include "utils.h"

typedef struct {
	strbuf_t *body_ret;
	strbuf_t *body;
	lua_State *L;
	int ret;
	uv_pipe_t *pipe_stdout;
	uv_process_t *proc;
} curl_t;

enum {
	RET_STRBUF = 1,
};

static uv_buf_t alloc_buffer(uv_handle_t *h, size_t len) {
	curl_t *p = (curl_t *)h->data;
	strbuf_ensure_empty_length(p->body_ret, len);
	return uv_buf_init(p->body_ret->buf, len);
}

static void pipe_handle_free(uv_handle_t *h) {
	curl_t *c = (curl_t *)h->data;
	free(h);

	lua_pushstring(c->L, c->body_ret->buf);
	lua_do_global_callback(c->L, "curl_done", c, 1, 1);
}

static void pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	curl_t *c = (curl_t *)st->data;

	if (n < 0) {
		uv_close((uv_handle_t *)c->pipe_stdout, pipe_handle_free);
		c->pipe_stdout = NULL;
		return;
	}

	strbuf_append_mem(c->body_ret, buf.base, n);
}

static void proc_handle_free(uv_handle_t *h) {
	free(h);
}

static void proc_on_exit(uv_process_t *proc, int stat, int sig) {
	curl_t *c = (curl_t *)proc->data;

	if (c->pipe_stdout) {
		uv_close((uv_handle_t *)c->pipe_stdout, pipe_handle_free);
		c->pipe_stdout = NULL;
	}

	uv_close((uv_handle_t *)proc, proc_handle_free);
}

// upvalue[1] = done
// curl_done_0x....(ret)
static int curl_done(lua_State *L) {
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, -2);
	lua_call_or_die(L, 1, 0);
	return 0;
}

// c = curl {
// 	 url = 'http://sugrsugr.com:8083',
// 	 ret = 'strbuf', -- return strbuf. default str
// 	 done = function (ret) end,
// 	 body = 'hello world'
// }
//
// c.cancel()
//
static int lua_curl(lua_State *L) {
	void *ud = lua_touserdata(L, lua_upvalueindex(1));
	uv_loop_t *loop;
	memcpy(&loop, ud, sizeof(loop));

	lua_getfield(L, 1, "url"); // 2
	lua_getfield(L, 1, "ret"); // 3
	lua_getfield(L, 1, "body"); // 4
	lua_getfield(L, 1, "done"); // 5

	char *url = (char *)lua_tostring(L, 2);
	if (url == NULL) {
		luaL_error(L, "url must set");
		exit(-1);
	}

	curl_t *c = (curl_t *)zalloc(sizeof(curl_t));
	c->L = L;

	char *ret = (char *)lua_tostring(L, 3);
	if (ret && strcmp(ret, "strbuf") == 0)
		c->ret = RET_STRBUF;

	char *body = (char *)lua_tostring(L, 4);
	if (body == NULL)
		body = "";

	c->body_ret = strbuf_new(4096);

	lua_pushvalue(L, 5);
	lua_pushcclosure(L, curl_done, 1);
	lua_set_global_callback(L, "curl_done", c);

	c->pipe_stdout = (uv_pipe_t *)zalloc(sizeof(uv_pipe_t));
	uv_pipe_init(loop, c->pipe_stdout, 0);
	uv_pipe_open(c->pipe_stdout, 0);
	c->pipe_stdout->data = c;

	c->proc = (uv_process_t *)zalloc(sizeof(uv_process_t));
	c->proc->data = c;

	uv_process_options_t opts = {};
	uv_stdio_container_t stdio[3] = {
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE|UV_READABLE_PIPE, .data.stream = (uv_stream_t *)c->pipe_stdout},
		{.flags = UV_IGNORE},
	};

	char *args[] = {"curl", url, "-d", body, NULL};
	opts.file = args[0];
	opts.args = args;
	opts.stdio = stdio;
	opts.stdio_count = 3;
	opts.exit_cb = proc_on_exit;

	info("body=%s", body);

	int r = uv_spawn(loop, c->proc, opts);
	info("spawn=%d pid=%d", r, c->proc->pid);

	uv_read_start((uv_stream_t *)c->pipe_stdout, alloc_buffer, pipe_read);

	return 1;
}

void lua_curl_init(lua_State *L, uv_loop_t *loop) {
	void *ud = lua_newuserdata(L, sizeof(loop));
	memcpy(ud, &loop, sizeof(loop));
	lua_pushcclosure(L, lua_curl, 1);
	lua_setglobal(L, "curl");
}

