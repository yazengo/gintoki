
#include <string.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"

void pipe_read(pipe_t *p, pipe_allocbuf_cb allocbuf, pipe_read_cb done) {
}

void pipe_write(pipe_t *p, uv_buf_t ub, pipe_write_cb done) {
}

typedef void (*pcopy_read_cb)(struct pcopy_s *c, uv_buf_t buf);
typedef void (*pcopy_write_cb)(struct pcopy_s *c);

typedef struct pcopy_s {
	pipe_t *src;
	pipe_t *sink;

	char rbuf[1024];
	char wbuf[1024];

	uv_buf_t wub;
	uv_write_t wr;

	pcopy_read_cb readdone;
	pcopy_write_cb writedone;
} pcopy_t;

static void pcopy_start(pipe_t *src, pipe_t *sink);
static void pcopy_read(pcopy_t *c, pcopy_read_cb done);
static void pcopy_write(pcopy_t *c, uv_buf_t ub, pcopy_write_cb done);
static void pcopy_readdone(pcopy_t *c, uv_buf_t buf);
static void pcopy_writedone(pcopy_t *c);
static void pcopy_close(pcopy_t *c);

static uv_buf_t pcopy_stream_allocbuf(uv_handle_t *h, size_t len) {
	info("h=%p", h);
	pipe_t *src = (pipe_t *)h->data;
	pcopy_t *c = src->cpy;

	if (src->type == PT_DIRECT)
		len = sizeof(uv_buf_t);
	else
		len = sizeof(c->rbuf);

	return uv_buf_init(c->rbuf, len);
}

static void pcopy_stream_writedone(uv_write_t *wr, int stat) {
	pipe_t *sink = (pipe_t *)wr->data;
	pcopy_t *c = sink->cpy;
	pipe_t *src = c->src;

	if (stat == -1) {
		pcopy_close(c);
		return;
	}

	pcopy_read(c, pcopy_readdone);
}

static void pcopy_stream_readdone(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	pipe_t *src = (pipe_t *)st->data;
	pcopy_t *c = src->cpy;
	pipe_t *sink = c->sink;
	uv_buf_t rbuf = {}, wbuf = {};

	uv_read_stop(st);

	if (n < 0) {
		pcopy_close(c);
		return;
	}
	buf.len = n;

	if (src->type == PT_DIRECT) {
		rbuf = *(uv_buf_t *)buf.base;
	} else {
		rbuf = buf;
	}

	c->readdone(c, wbuf);
}

static void pcopy_read(pcopy_t *c, pcopy_read_cb done) {
	pipe_t *src = c->src;
	pipe_t *sink = c->sink;

	c->readdone = done;

	if (src->type == PT_STREAM) {
		src->st->data = src;
		uv_read_start(src->st, pcopy_stream_allocbuf, pcopy_stream_readdone);
	} else if (src->type == PT_DIRECT) {
	} else {
		panic("file type unsupported");
	}
}

static void pcopy_stream_write(pcopy_t *c, uv_buf_t ub) {
	pipe_t *sink = c->sink;

	sink->st->data = sink;
	c->wub = ub;
	c->wr.data = sink;
	uv_write(&c->wr, sink->st, &c->wub, 1, pcopy_stream_writedone);
}

static void pcopy_direct_writedone(void *_c) {
	pcopy_t *c = (pcopy_t *)_c;

	c->writedone(c);
}

static void pcopy_direct_write(pcopy_t *c, uv_buf_t ub) {
	pipe_t *sink = c->sink;

	set_immediate(pcopy_direct_writedone, c);
}

static void pcopy_write(pcopy_t *c, uv_buf_t ub, pcopy_write_cb done) {
	pipe_t *sink = c->sink;

	c->writedone = done;

	if (sink->type == PT_DIRECT) {
		pcopy_stream_write(c, ub);
	} else if (sink->type == PT_STREAM) {
		pcopy_direct_write(c, ub);
	} else {
		panic("file type unsupported");
	}
}

static void pcopy_close(pcopy_t *c) {
	luv_unref(c->sink);
	luv_unref(c->src);
}

static void pcopy_writedone(pcopy_t *c) {
	pcopy_read(c, pcopy_readdone);
}

static void pcopy_readdone(pcopy_t *c, uv_buf_t buf) {
	pcopy_write(c, buf, pcopy_writedone);
}

static void pcopy_start(pipe_t *src, pipe_t *sink) {
	pcopy_t *c = (pcopy_t *)zalloc(sizeof(pcopy_t));

	c->src = src;
	c->sink = sink;
	src->cpy = c;
	sink->cpy = c;

	luv_ref(src);
	luv_ref(sink);

	pcopy_read(c, pcopy_readdone);
}

static int luv_pcopy(lua_State *L, uv_loop_t *loop) {
	pipe_t *src = (pipe_t *)luv_toctx(L, 1);
	pipe_t *sink = (pipe_t *)luv_toctx(L, 2);
	pcopy_start(src, sink);
	return 1;
}

void luv_pipe_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pcopy", luv_pcopy);
}

