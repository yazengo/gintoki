
#include <string.h>
#include <stdlib.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"
#include "pdirect.h"

static void gc(uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;
	debug("p=%p", p);
	if (p->gc)
		p->gc(p);
}

pipe_t *pipe_new(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	luv_setgc(p, gc);
	return p;
}

void pipe_read(pipe_t *p, pipe_read_cb done) {
	p->read.done = done;

	debug("read type=%d", p->type);

	switch (p->type) {
	case PSTREAM_SRC:
		pstream_read(p);
		break;

	case PDIRECT_SRC:
	case PDIRECT_SINK:
		pdirect_read(p);
		break;

	default:
		panic("type=%d invalid", p->type);
	}
}

void pipe_write(pipe_t *p, pipebuf_t *pb, pipe_write_cb done) {
	p->write.pb = pb;
	p->write.done = done;

	debug("type=%d", p->type);

	switch (p->type) {
	case PSTREAM_SINK:
		pstream_write(p);
		break;

	case PDIRECT_SRC:
	case PDIRECT_SINK:
		pdirect_write(p);
		break;

	default:
		panic("type=%d invalid", p->type);
	}
}

void pipe_close_read(pipe_t *p) {
	switch (p->type) {
	case PSTREAM_SRC:
		pstream_close(p);
		break;

	case PDIRECT_SRC:
	case PDIRECT_SINK:
		pdirect_close_read(p);
		break;

	default:
		panic("type=%d invalid", p->type);
	}
}

void pipe_close_write(pipe_t *p) {
	switch (p->type) {
	case PSTREAM_SINK:
		pstream_close(p);
		break;

	case PDIRECT_SRC:
	case PDIRECT_SINK:
		pdirect_close_write(p);
		break;

	default:
		panic("type=%d invalid", p->type);
	}
}

void pipe_cancel_read(pipe_t *p) {
	switch (p->type) {
	case PSTREAM_SRC:
		pstream_cancel_read(p);
		break;

	case PDIRECT_SRC:
	case PDIRECT_SINK:
		pdirect_cancel_read(p);
		break;

	default:
		panic("type=%d invalid", p->type);
	}
}

void pipe_cancel_write(pipe_t *p) {
	switch (p->type) {
	case PSTREAM_SINK:
		pstream_cancel_write(p);
		break;

	case PDIRECT_SRC:
	case PDIRECT_SINK:
		pdirect_cancel_write(p);
		break;

	default:
		panic("type=%d invalid", p->type);
	}
}

static int pclose_read(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = (pipe_t *)luv_toctx(L, 1);
	pipe_close_read(p);
	return 0;
}

static int pclose_write(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = (pipe_t *)luv_toctx(L, 1);
	pipe_close_write(p);
	return 0;
}

static int pipe_setopt(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = (pipe_t *)luv_toctx(L, 1);
	char *op = (char *)lua_tostring(L, 2);

	if (op && !strcmp(op, "read_mode")) {
		char *mode = (char *)lua_tostring(L, 3);
		if (mode && !strcmp(mode, "block"))
			p->read.mode = PREAD_BLOCK;
		return 0;
	}

	return 0;
}

void luv_pipe_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pclose_read", pclose_read);
	luv_register(L, loop, "pclose_write", pclose_write);
	luv_register(L, loop, "pipe_setopt", pipe_setopt);
}

