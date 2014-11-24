
#include <string.h>
#include <stdlib.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"
#include "pdirect.h"

pipe_t *pipe_new(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
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

