
#include "utils.h"

typedef struct {
	strbuf_t *sb;
	lua_State *L;
	int ret;
} proc_t;

enum {
	RET_STRBUF = 1,
};

static uv_buf_t alloc_buffer(uv_handle_t *h, size_t len) {
	proc_t *p = (proc_t *)h->data;
	strbuf_ensure_empty_length(p->sb, len);
	return uv_buf_init(p->sb->buf, len);
}

static void handle_free(uv_handle_t *h, int stat) {
	free(h);
}

static void pipe_read(uv_stream_t *st, ssize_t nread, uv_buf_t buf) {
	proc_t *p = (proc_t *)((uv_handle_t *)st)->data;
	if (nread >= 0)
		strbuf_append_mem(p->sb, buf.base, nread);
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
	proc_t *p = (proc_t *)puv->data;
	
	strbuf_append_char(p->sb, '\x00');
	info("%s", p->sb->buf);

	//json_decode_from_buf(L, p->sb->buf, p->sb->length);

	strbuf_free(p->sb);
	free(p);
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

	proc_t *p = (proc_t *)zalloc(sizeof(proc_t));
	p->sb = strbuf_new(4096);

	lua_getfield(L, 1, "done");
	lua_pushcclosure(L, curl_done, 1);
	char name[128];
	sprintf(name, "curl_done_%p", p);
	lua_setglobal(L, name);

	lua_getfield(L, 1, "body");
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

