
#include <string.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"
#include "pcopy.h"

typedef struct pcopy_s {
	pipe_t *src;
	pipe_t *sink;
	unsigned flags;
} pcopy_t;

enum {
	DONT_CLOSE_WRITE = (1<<0),
};

static void copy(pcopy_t *c);

static void close_all(pcopy_t *c) {
	debug("close");

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
	copy(c);
}

static void read_done(pipe_t *src, pipebuf_t *pb) {
	pcopy_t *c = src->copy;

	if (pb == NULL) {
		debug("read eof");
		close_all(c);
		return;
	}

	debug("done");
	pipe_write(c->sink, pb, write_done);
}

static void copy(pcopy_t *c) {
	debug("read");
	pipe_read(c->src, read_done);
}

static int pcopy_close(lua_State *L, uv_loop_t *loop, void *_c) {
	pcopy_t *c = (pcopy_t *)_c;

	close_all(c);
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

	luv_pushcclosure(L, pcopy_close, c);
	lua_setfield(L, -2, "close");

	char *m = mode;
	while (m && *m) {
		if (*m == 'b')
			src->read.mode = PREAD_BLOCK;
		else if (*m == 'w')
			c->flags |= DONT_CLOSE_WRITE;
		m++;
	}

	copy(c);

	return 1;
}

void luv_pcopy_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pcopy", luv_pcopy);
}

