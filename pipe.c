
#include "luv.h"
#include "utils.h"
#include "pipe.h"

enum {
	PT_STREAM,
	PT_DIRECT
};

static void pipecopy_readdone(uv_stream_t *st, ssize_t n, uv_buf_t buf);

static void pipecopy_close(pipecopy_t *c) {
	luv_unref(c->sink);
	luv_unref(c->src);
}

static uv_buf_t pipecopy_allocbuf(uv_handle_t *h, size_t len) {
	pipe_t *src = (pipe_t *)h->data;
	pipecopy_t *c = src->cpy;

	if (src->type == PT_DIRECT)
		len = sizeof(uv_buf_t);
	else
		len = sizeof(c->rbuf);

	return uv_buf_init(c->rbuf, len);
}

static void pipecopy_writedone(uv_write_t *wr, int stat) {
	pipe_t *sink = (pipe_t *)wr->data;
	pipecopy_t *c = sink->cpy;
	pipe_t *src = c->src;

	if (stat == -1) {
		pipecopy_close(c);
		return;
	}

	uv_read_start(src->st, pipecopy_allocbuf, pipecopy_readdone);
}

static void pipecopy_readdone(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	pipe_t *src = (pipe_t *)st->data;
	pipecopy_t *c = src->cpy;
	pipe_t *sink = c->sink;
	uv_buf_t rbuf = {}, wbuf = {};

	uv_read_stop(st);

	info("n=%d", n);

	if (n < 0) {
		pipecopy_close(c);
		return;
	}

	buf.len = n;

	if (src->type == PT_DIRECT) {
		rbuf = *(uv_buf_t *)buf.base;
	} else {
		rbuf = buf;
	}

	if (sink->type == PT_DIRECT) {
		memcpy(c->wbuf, &rbuf, sizeof(rbuf));
		wbuf.base = c->wbuf;
		wbuf.len = sizeof(rbuf);
	} else {
		wbuf = rbuf;
	}

	c->wub = wbuf;
	c->wr.data = sink;
	uv_write(&c->wr, sink->st, &c->wub, 1, pipecopy_writedone);
}

void uv_pipecopy_start(pipe_t *src, pipe_t *sink) {
	pipecopy_t *c = (pipecopy_t *)zalloc(sizeof(pipecopy_t));

	c->src = src;
	c->sink = sink;
	src->st->data = src;
	sink->st->data = sink;
	src->cpy = c;
	sink->cpy = c;

	luv_ref(src);
	luv_ref(sink);

	uv_read_start(src->st, pipecopy_allocbuf, pipecopy_readdone);
}

static int luv_pipecopy(lua_State *L, uv_loop_t *loop) {
	pipe_t *src = (pipe_t *)luv_toctx(L, 1);
	pipe_t *sink = (pipe_t *)luv_toctx(L, 2);
	uv_pipecopy_start(src, sink);
	return 1;
}

void luv_pipe_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pcopy", luv_pipecopy);
}

