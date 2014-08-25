
#include "utils.h"

typedef struct {
	strbuf_t *body_ret;
	strbuf_t *body;
	lua_State *L;
	int ret;
} curl_t;

enum {
	RET_STRBUF = 1,
};

static uv_buf_t alloc_buffer(uv_handle_t *h, size_t len) {
	curl_t *p = (curl_t *)h->data;
	strbuf_ensure_empty_length(p->body_ret, len);
	return uv_buf_init(p->body_ret->buf, len);
}

static void handle_free(uv_handle_t *h, int stat) {
	free(h);
}

static void pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	curl_t *p = (curl_t *)((uv_handle_t *)st)->data;
	if (n >= 0)
		strbuf_append_mem(p->body_ret, buf.base, n);
	else
		uv_close((uv_handle_t *)st, handle_free);
}

// upvalue[1] = done
// curl_done_0x....(ret)
static int curl_done(lua_State *L) {
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_replace(L, -2);
	lua_call_or_die(L, 1, 0);
	return 0;
}

static void proc_on_exit(uv_process_t *puv, int stat, int sig) {
	curl_t *p = (curl_t *)puv->data;
	
	strbuf_append_char(p->sb, '\x00');
	info("%s", p->sb->buf);

	//json_decode_from_buf(L, p->sb->buf, p->sb->length);

	strbuf_free(p->sb);
	free(p);
}

static strbuf_t *lua_body_to_strbuf(lua_State *L) {
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return NULL;
	}

	strbuf_t *body = strbuf_new(128);
	if (!lua_isnil(L, -1)) {
		if (lua_isstring(L, -1)) {
			strbuf_append_string(body, lua_tostring(L, -1));
		}
		if (lua_istable(L, -1)) {
		}
	}

	return body;
}

// curl{
// 	 url = 'http://sugrsugr.com:8083',
// 	 ret = 'strbuf', -- return strbuf
// 	 done = function (ret) end,
// 	 body = {a='c',b='d'}, -- encoding to json
// }
static int lua_curl(lua_State *L) {
	void *ud = lua_touserdata(L, lua_upvalueindex(L, 1));
	uv_loop_t *loop;
	memcpy(&loop, ud, sizeof(loop));

	char *url;
	lua_getfield(L, 1, "url");
	url = lua_tostring(L, -1);

	if (url == NULL) {
		warn("failed: url=nil");
		return 0;
	}

	curl_t *c = (curl_t *)zalloc(sizeof(curl_t));

	lua_getfield(L, 1, "body");
	c->body = lua_body_to_strbuf(L);
	c->body_ret = strbuf_new(4096);

	lua_getfield(L, 1, "done");
	lua_pushcclosure(L, curl_done, 1);
	lua_set_global_callback(L, "curl_done", p);

	// TODO: cont

	uv_pipe_t *pipe = (uv_pipe_t *)zalloc(sizeof(uv_pipe_t));
	uv_pipe_open(pipe, 0);

	uv_process_t *puv = (uv_process_t *)zalloc(sizeof(uv_process_t));

	puv->data = p;
	pipe->data = p;

	uv_process_options_t opts = {};
	uv_stdio_container_t stdio[3] = {
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE|UV_READABLE_PIPE, .data.stream = (uv_stream_t *)pipe},
		{.flags = UV_IGNORE},
	};

	char *args[] = {"echo", "{\"op\":\"testing\"}", NULL};
	opts.file = args[0];
	opts.args = args;
	opts.stdio = stdio;
	opts.stdio_count = 3;
	opts.exit_cb = proc_on_exit;

	int r = uv_spawn(loop, puv, opts);
	info("spawn=%d", r);

	uv_read_start((uv_stream_t *)pipe, alloc_buffer, pipe_read);

	return 0;
}

void lua_curl_init(lua_State *L, uv_loop_t *loop) {
	void *ud = lua_newuserdata(L, sizeof(loop));
	memcpy(ud, &loop, sizeof(loop));
	lua_pushcclosure(L, lua_curl, 1);
	lua_setglobal(L, "curl");
}

