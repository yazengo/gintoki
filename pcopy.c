
#include <string.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"
#include "pcopy.h"

static void copy(pcopy_t *c);

static void close_all(pcopy_t *c) {
	debug("close");
	pipe_close_write(c->sink);
	pipe_close_read(c->src);
	luv_unref(c);
}

static void write_done(pipe_t *sink, int stat) {
	pcopy_t *c = sink->copy;

	if (stat < 0) {
		close_all(c);
		return;
	}
	copy(c);
}

static void read_done(pipe_t *src, ssize_t n, uv_buf_t ub) {
	pcopy_t *c = src->copy;

	if (n < 0) {
		close_all(c);
		return;
	}

	debug("n=%d", n);
	pipe_write(c->sink, ub, write_done);
}

static void copy(pcopy_t *c) {
	pipe_read(c->src, NULL, read_done);
}

static int luv_pcopy(lua_State *L, uv_loop_t *loop) {
	pipe_t *src = (pipe_t *)luv_toctx(L, 1);
	pipe_t *sink = (pipe_t *)luv_toctx(L, 2);
	pcopy_t *c = (pcopy_t *)luv_newctx(L, loop, sizeof(pcopy_t));

	c->src = src;
	c->sink = sink;
	src->copy = c;
	sink->copy = c;

	copy(c);

	return 1;
}

void luv_pcopy_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pcopy", luv_pcopy);
}

