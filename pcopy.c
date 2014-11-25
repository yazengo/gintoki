
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
	immediate_t im;
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
	DONT_CLOSE_WRITE = (1<<0),
	FIRST_CB         = (1<<1),
};

static void copy(pcopy_t *c);

static void close_all(pcopy_t *c) {
	debug("close");

	c->stat = CLOSED;

	if (!(c->flags & DONT_CLOSE_WRITE))
		pipe_close_write(c->sink);
	pipe_close_read(c->src);

	luv_callfield(c, "done_cb", 0, 0);
	luv_unref(c);
}

static void write_done(pipe_t *sink, int stat) {
	pcopy_t *c = sink->copy;

	if (stat < 0) {
		debug("write eof");
		close_all(c);
		return;
	}

	c->tx += PIPEBUF_SIZE;

	if (c->stat != WRITING)
		panic("stat=%d invalid", c->stat);
	c->stat = INIT;

	copy(c);
}

static void read_done(pipe_t *src, pipebuf_t *pb) {
	pcopy_t *c = src->copy;

	if (pb == NULL) {
		debug("read eof");
		close_all(c);
		return;
	}

	if (c->rx == 0 && (c->flags & FIRST_CB)) {
		luv_callfield(c, "first_cb", 0, 0);
	}

	c->rx += PIPEBUF_SIZE;

	if (c->stat != READING)
		panic("stat=%d invalid", c->stat);
	c->stat = WRITING;

	debug("done");
	pipe_write(c->sink, pb, write_done);
}

static void copy(pcopy_t *c) {
	if (c->stat != INIT)
		panic("stat=%d invalid", c->stat);
	c->stat = READING;

	debug("read");
	pipe_read(c->src, read_done);
}

static void im_close(immediate_t *im) {
	pcopy_t *c = (pcopy_t *)im->data;

	debug("close");
	close_all(c);
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
	c->im.data = c;
	c->im.cb = im_close;
	set_immediate(luv_loop(c), &c->im);
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
	src->copy = c;
	sink->copy = c;

	luv_pushcclosure(L, pcopy_setopt, c);
	lua_setfield(L, -2, "setopt");

	char *m = mode;
	while (m && *m) {
		switch (*m) {
		case 'b':
			src->read.mode = PREAD_BLOCK;
			break;
		case 'w':
			c->flags |= DONT_CLOSE_WRITE;
			break;
		}
		m++;
	}

	copy(c);

	return 1;
}

void luv_pcopy_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pcopy", luv_pcopy);
}

