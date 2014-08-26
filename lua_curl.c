
#include <stdlib.h>
#include <errno.h>
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>

#include "strbuf.h"
#include "utils.h"

typedef struct {
	lua_State *L;
	uv_loop_t *loop;

	int cancelled;
	int closed_nr;

	int rxbytes;

	int code;
	int exit_sig, exit_code;

	strbuf_t *retsb;
	uv_fs_t *retfp_req;
	uv_file retfp_file;
	int64_t retfp_off;
	char retbuf[4096];
	int ret;

	uv_pipe_t *pipe_stdout;
	uv_process_t *proc;
} curl_t;

enum {
	RET_STRBUF = 1,
};

static void on_retfp_write_done(uv_fs_t *r);
static void check_close(curl_t *c);

static void do_cancel(curl_t *c) {
	c->cancelled = 1;

	lua_pushnil(c->L);
	lua_set_global_callback(c->L, "curl_done", c);

	uv_process_kill(c->proc, 9);
}

static uv_buf_t alloc_buffer(uv_handle_t *h, size_t len) {
	curl_t *c = (curl_t *)h->data;

	if (c->retsb) {
		strbuf_ensure_empty_length(c->retsb, len);
		return uv_buf_init(c->retsb->buf + c->retsb->length, len);
	}
	return uv_buf_init(c->retbuf, sizeof(c->retbuf));
}

static void on_retfp_close(uv_fs_t *r) {
	curl_t *c = (curl_t *)r->data;

	debug("file closed");
	check_close(c);
}

static void check_close(curl_t *c) {
	c->closed_nr++;
	if (c->closed_nr < 2) 
		return;

	if (c->retfp_file) {
		uv_fs_close(c->loop, c->retfp_req, c->retfp_file, on_retfp_close);
		c->retfp_file = 0;
		return;
	}

	debug("close all");

	if (c->cancelled) 
		c->code = -ECANCELED;
	if (c->exit_sig)
		c->code = -EINVAL;
	if (c->exit_code)
		c->code = -EINVAL;

	if (c->retsb) {
		strbuf_append_mem(c->retsb, "", 1);
		lua_pushstring(c->L, c->retsb->buf);
	} else
		lua_pushnil(c->L);

	lua_pushnumber(c->L, c->code);
	lua_do_global_callback(c->L, "curl_done", c, 2, 1);

	if (c->retsb)
		strbuf_free(c->retsb);
	if (c->retfp_req) {
		uv_fs_req_cleanup(c->retfp_req);
		free(c->retfp_req);
	}
	free(c);
}

static void handle_free(uv_handle_t *h) {
	curl_t *c = (curl_t *)h->data;
	free(h);

	debug("freed");
	check_close(c);
}

static void on_stdout_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	curl_t *c = (curl_t *)st->data;

	debug("n=%d", n);

	if (n < 0) {
		debug("free pipe_stdout");
		uv_close((uv_handle_t *)c->pipe_stdout, handle_free);
		c->pipe_stdout = NULL;
		return;
	}

	if (c->retsb) {
		strbuf_append_mem(c->retsb, buf.base, n);
	}

	if (c->retfp_req) {
		uv_read_stop(st);
		uv_fs_req_cleanup(c->retfp_req);
		uv_fs_write(c->loop, c->retfp_req, c->retfp_file, buf.base, n, c->retfp_off, on_retfp_write_done);
		c->retfp_off += n;
	}
}

static void on_retfp_write_done(uv_fs_t *r) {
	curl_t *c = (curl_t *)r->data;

	debug("ok");

	uv_read_start((uv_stream_t *)c->pipe_stdout, alloc_buffer, on_stdout_read);
}

static void proc_on_exit(uv_process_t *proc, int stat, int sig) {
	curl_t *c = (curl_t *)proc->data;
	
	debug("exit sig=%d stat=%d", sig, stat);

	c->exit_sig = sig;
	c->exit_code = stat;

	if (c->pipe_stdout) {
		debug("free stdout");
		uv_read_stop((uv_stream_t *)c->pipe_stdout);
		uv_close((uv_handle_t *)c->pipe_stdout, handle_free);
		c->pipe_stdout = NULL;
	}

	debug("free proc");
	uv_close((uv_handle_t *)proc, handle_free);
}

static void on_retfp_open(uv_fs_t *r) {
	curl_t *c = (curl_t *)r->data;

	int fd = r->result;
	uv_fs_req_cleanup(r);

	if (fd < 0) {
		warn("retfp=%d open failed: %s", fd, uv_strerror(uv_last_error(c->loop)));
		do_cancel(c);
		return;
	}

	debug("ok fd=%d", fd);
	c->retfp_file = fd;

	uv_read_start((uv_stream_t *)c->pipe_stdout, alloc_buffer, on_stdout_read);
}

// upvalue[1] = done
// curl_done_0x....(ret, code)
static int curl_done(lua_State *L) {
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_insert(L, -3);
	lua_call_or_die(L, 2, 0);
	return 0;
}

// upvalue[1] = curl_t *
// curl_cancel()
static int curl_cancel(lua_State *L) {
	curl_t *c = (curl_t *)lua_touserptr(L, lua_upvalueindex(1));
	do_cancel(c);
	return 0;
}

// upvalue[1] = curl_t *
// curl_info() = { rxbytes }
static int curl_info(lua_State *L) {
	curl_t *c = (curl_t *)lua_touserptr(L, lua_upvalueindex(1));

	lua_newtable(L);
	lua_pushnumber(L, c->rxbytes);
	lua_setfield(L, -2, "rxbytes");

	return 1;
}

//
// c = curl {
// 	 url = 'http://sugrsugr.com:8083',
// 	 ret = 'strbuf', -- return strbuf. default str
// 	 done = function (ret, code) 
// 	 		code = 200 -- HTTP 200 OK
// 	 		code = 403 -- HTTP 403
// 	 		code = -ECANCELED -- user canceled
// 	 		code = -ENOENT -- file open failed
// 	 end,
// 	 body = 'hello world'
//
// 	 tofile = '/file/path',
// }
//
// c.cancel()
// c.progress() -- 87.53
//
static int lua_curl(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	lua_getfield(L, 1, "url"); // 2
	lua_getfield(L, 1, "ret"); // 3
	lua_getfield(L, 1, "body"); // 4
	lua_getfield(L, 1, "done"); // 5
	lua_getfield(L, 1, "tofile"); // 6
	lua_getfield(L, 1, "progress"); // 7

	char *url = (char *)lua_tostring(L, 2);
	if (url == NULL) {
		luaL_error(L, "url must set");
		exit(-1);
	}

	curl_t *c = (curl_t *)zalloc(sizeof(curl_t));
	c->L = L;
	c->loop = loop;

	char *ret = (char *)lua_tostring(L, 3);
	if (ret && strcmp(ret, "strbuf") == 0)
		c->ret = RET_STRBUF;

	char *req_s = (char *)lua_tostring(L, 4);

	lua_pushvalue(L, 5);
	lua_pushcclosure(L, curl_done, 1);
	lua_set_global_callback(L, "curl_done", c);

	c->pipe_stdout = (uv_pipe_t *)zalloc(sizeof(uv_pipe_t));
	uv_pipe_init(loop, c->pipe_stdout, 0);
	uv_pipe_open(c->pipe_stdout, 0);
	c->pipe_stdout->data = c;

	c->proc = (uv_process_t *)zalloc(sizeof(uv_process_t));
	c->proc->data = c;

	char *tofile = (char *)lua_tostring(L, 6);
	if (tofile) {
		c->retfp_req = (uv_fs_t *)zalloc(sizeof(uv_fs_t));
		c->retfp_req->data = c;
		info("fs_open: %s", tofile);
		uv_fs_open(loop, c->retfp_req, tofile, O_WRONLY|O_CREAT|O_TRUNC, 0777, on_retfp_open);
	} else {
		c->retsb = strbuf_new(4096);
	}

	uv_process_options_t opts = {};
	uv_stdio_container_t stdio[3] = {
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE|UV_READABLE_PIPE, .data.stream = (uv_stream_t *)c->pipe_stdout},
		{.flags = UV_IGNORE},
	};

	char *args_d[] = {"curl", "-v", url, "-d", req_s, NULL};
	char *args_without_d[] = {"curl", "-v", url, NULL};
	char **args;

	if (req_s)
		args = args_d;
	else
		args = args_without_d;

	opts.file = args[0];
	opts.args = args;
	opts.stdio = stdio;
	opts.stdio_count = 3;
	opts.exit_cb = proc_on_exit;

	info("req_s=%s", req_s);

	// r = {
	// 	 cancel = [native function],
	// 	 info = [native function],
	// }
	lua_newtable(L);

	lua_pushuserptr(L, c);
	lua_pushcclosure(L, curl_cancel, 1);
	lua_setfield(L, -2, "cancel");

	lua_pushuserptr(L, c);
	lua_pushcclosure(L, curl_info, 1);
	lua_setfield(L, -2, "info");

	int r = uv_spawn(loop, c->proc, opts);
	info("spawn=%d pid=%d", r, c->proc->pid);

	if (c->retsb)
		uv_read_start((uv_stream_t *)c->pipe_stdout, alloc_buffer, on_stdout_read);

	return 1;
}

void lua_curl_init(lua_State *L, uv_loop_t *loop) {
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_curl, 1);
	lua_setglobal(L, "curl");
}

