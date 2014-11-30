
#include <string.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"
#include "pcopy.h"

typedef struct pcopy_s {
	pipe_t *src;
	pipe_t *sink;
	unsigned flags;
	int stat;
	int tx, rx;
	immediate_t im_close;
	immediate_t im_first_cb;
} pcopy_t;

enum {
	INIT,
	READING,
	WRITING,
	PAUSED,
	CLOSING,
	CLOSED,
};

enum {
	NEED_CLOSE_WRITE = (1<<0),
	NEED_CLOSE_READ  = (1<<1),
	FIRST_CB         = (3<<2),
};

static void copy(pcopy_t *c);

static void close_all(pcopy_t *c, const char *reason) {
	debug("c=%p", c);

	c->stat = CLOSED;

	if (c->flags & NEED_CLOSE_WRITE)
		pipe_close_write(c->sink);
	if (c->flags & NEED_CLOSE_READ)
		pipe_close_read(c->src);

	debug("c=%p reason=%s", c, reason);
	lua_pushstring(luv_state(c), reason);
	luv_callfield(c, "done_cb", 1, 0);
	luv_unref(c);
}

static void write_done(pipe_t *sink, int stat) {
	pcopy_t *c = (pcopy_t *)sink->write.data;

	if (stat < 0) {
		debug("eof c=%p", c);
		close_all(c, "w");
		return;
	}

	c->tx += PIPEBUF_SIZE;

	if (c->stat != WRITING)
		panic("c=%p c.stat=%d p=%p invalid", c, c->stat, sink);
	c->stat = INIT;

	copy(c);
}

static void im_first_cb(immediate_t *im) {
	pcopy_t *c = (pcopy_t *)im->data;

	luv_callfield(c, "first_cb", 0, 0);
}

static void read_done(pipe_t *src, pipebuf_t *pb) {
	pcopy_t *c = (pcopy_t *)src->read.data;

	if (pb == NULL) {
		debug("eof c=%p", c);
		close_all(c, "r");
		return;
	}

	if (c->rx == 0 && (c->flags & FIRST_CB)) {
		c->im_first_cb.data = c;
		c->im_first_cb.cb = im_first_cb;
		set_immediate(luv_loop(c), &c->im_first_cb);
	}

	c->rx += PIPEBUF_SIZE;

	if (c->stat != READING)
		panic("stat=%d invalid", c->stat);
	c->stat = WRITING;

	debug("write c=%p p=%p", c->sink);
	pipe_write(c->sink, pb, write_done);
}

static void copy(pcopy_t *c) {
	if (c->stat != INIT)
		panic("stat=%d invalid", c->stat);
	c->stat = READING;

	debug("read c=%p p=%p", c, c->src);
	pipe_read(c->src, read_done);
}

static void im_close(immediate_t *im) {
	pcopy_t *c = (pcopy_t *)im->data;

	close_all(c, "c");
}

static void pcopy_close(pcopy_t *c) {
	switch (c->stat) {
	case READING:
		pipe_cancel_read(c->src);
		break;

	case WRITING:
		pipe_cancel_write(c->sink);
		break;

	case CLOSED:
	case CLOSING:
		return;
	}

	c->stat = CLOSING;
	c->im_close.data = c;
	c->im_close.cb = im_close;
	set_immediate(luv_loop(c), &c->im_close);
}

static int pcopy_pause(pcopy_t *c) {
	switch (c->stat) {
	case PAUSED:
		return 0;

	case READING:
		pipe_cancel_read(c->src);
		c->stat = PAUSED;
		return 1;

	case WRITING:
		pipe_cancel_write(c->sink);
		c->stat = PAUSED;
		return 1;
	}
	return 0;
}

static int pcopy_resume(pcopy_t *c) {
	switch (c->stat) {
	case PAUSED:
		c->stat = INIT;
		copy(c);
		return 1;
	}
	return 0;
}

static int pcopy_setopt(lua_State *L, uv_loop_t *loop, void *_c) {
	pcopy_t *c = (pcopy_t *)_c;
	char *op = (char *)lua_tostring(L, 1);

	if (op && !strcmp(op, "pause")) {
		lua_pushboolean(L, pcopy_pause(c));
		return 1;
	}

	if (op && !strcmp(op, "resume")) {
		lua_pushboolean(L, pcopy_resume(c));
		return 1;
	}

	if (op && !strcmp(op, "close")) {
		pcopy_close(c);
		return 0;
	}

	if (op && !strcmp(op, "get.rx")) {
		lua_pushnumber(L, c->rx);
		return 1;
	}

	if (op && !strcmp(op, "get.tx")) {
		lua_pushnumber(L, c->tx);
		return 1;
	}

	if (op && !strcmp(op, "first_cb")) {
		luv_pushctx(L, c);
		lua_pushvalue(L, 2);
		lua_setfield(L, -2, "first_cb");
		c->flags |= FIRST_CB;
		return 0;
	}

	return 0;
}

// pcopy(src, sink, 'b')
static int luv_pcopy(lua_State *L, uv_loop_t *loop) {
	pipe_t *src = (pipe_t *)luv_toctx(L, 1);
	pipe_t *sink = (pipe_t *)luv_toctx(L, 2);
	char *mode = (char *)lua_tostring(L, 3);
	pcopy_t *c = (pcopy_t *)luv_newctx(L, loop, sizeof(pcopy_t));

	c->src = src;
	c->sink = sink;

	src->read.data = c;
	sink->write.data = c;

	luv_pushcclosure(L, pcopy_setopt, c);
	lua_setfield(L, -2, "setopt");

	char *m = mode;
	while (m && *m) {
		switch (*m) {
		case 'b':
			src->read.mode = PREAD_BLOCK;
			break;
		case 'w':
			c->flags |= NEED_CLOSE_WRITE;
			break;
		case 'r':
			c->flags |= NEED_CLOSE_READ;
			break;
		}
		m++;
	}

	debug("new c=%p src=%p sink=%p", c, src, sink);
	copy(c);

	return 1;
}

void luv_pcopy_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pcopy", luv_pcopy);
}

