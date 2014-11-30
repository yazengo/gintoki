
#include <string.h>
#include <stdlib.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"
#include "pdirect.h"

enum {
	INIT,
	READING,
	WRITING,
	CLOSING_READ,
	CLOSING_WRITE,
	CLOSED_READ,
	CLOSED_WRITE,
	CLOSED,
};

#define debugstat(p) debug("p=%p stat=%d wrstat=%d type=%d", p, p->stat, p->wrstat, p->type)
#define panicstat(p) panic("invalid: p=%p stat=%d wrstat=%d type=%d", p, p->stat, p->wrstat, p->type)

static void do_read(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;

	debugstat(p);
	if (p->rdstat != READING)
		panicstat(p);
	p->rdstat = INIT;

	switch (p->stat) {
	case WRITING:
		p->write.done(p, 0);
		p->read.done(p, p->direct.pool);
		p->stat = INIT;
		break;

	case CLOSING_WRITE:
		p->read.done(p, p->direct.pool);
		p->stat = CLOSED_WRITE;
		break;

	case INIT:
		p->stat = READING;
		break;
	
	case CLOSED_WRITE:
		p->read.done(p, NULL);
		break;

	default:
		panicstat(p);
	}
}

void pdirect_read(pipe_t *p) {
	debugstat(p);
	if (p->rdstat != INIT)
		panicstat(p);
	p->rdstat = READING;

	p->read.im_direct.data = p;
	p->read.im_direct.cb = do_read;
	set_immediate(luv_loop(p), &p->read.im_direct);
}

static void do_write(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;

	debugstat(p);
	if (p->wrstat != WRITING)
		panicstat(p);
	p->wrstat = INIT;

	switch (p->stat) {
	case READING:
		p->read.done(p, p->write.pb);
		p->write.done(p, 0);
		p->stat = INIT;
		break;

	case INIT:
		p->direct.pool = p->write.pb;
		p->stat = WRITING;
		break;

	case CLOSED_READ:
		p->write.done(p, -1);
		break;

	default:
		panicstat(p);
	}
}

void pdirect_write(pipe_t *p) {
	debugstat(p);

	if (p->wrstat != INIT)
		panicstat(p);
	p->wrstat = WRITING;

	p->write.im_direct.data = p;
	p->write.im_direct.cb = do_write;
	set_immediate(luv_loop(p), &p->write.im_direct);
}

static void do_close(pipe_t *p) {
	p->stat = CLOSED;
	luv_unref(p);
}

static void close_read(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;

	debugstat(p);
	if (p->rdstat != CLOSING_READ)
		panicstat(p);
	p->rdstat = INIT;

	switch (p->stat) {
	case INIT:
		p->stat = CLOSED_READ;
		break;

	case READING:
		p->stat = CLOSED_READ;
		break;

	case WRITING:
		pipebuf_unref(p->direct.pool);
		p->write.done(p, -1);
		p->stat = CLOSED_READ;
		break;

	case CLOSING_WRITE:
		pipebuf_unref(p->direct.pool);
		do_close(p);
		break;

	case CLOSED_WRITE:
		do_close(p);
		break;

	default:
		panicstat(p);
	}
}

void pdirect_close_read(pipe_t *p) {
	pdirect_cancel_read(p);

	if (p->rdstat != INIT)
		panicstat(p);
	p->rdstat = CLOSING_READ;

	p->close_read.im.data = p;
	p->close_read.im.cb = close_read;
	set_immediate(luv_loop(p), &p->close_read.im);
}

static void close_write(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;

	debugstat(p);
	if (p->wrstat != CLOSING_WRITE)
		panicstat(p);
	p->wrstat = INIT;

	switch (p->stat) {
	case INIT:
		p->stat = CLOSED_WRITE;
		break;

	case READING:
		p->read.done(p, NULL);
		p->stat = CLOSED_WRITE;
		break;

	case WRITING:
		p->stat = CLOSING_WRITE;
		break;

	case CLOSED_READ:
		do_close(p);
		break;

	default:
		panicstat(p);
	}
}

void pdirect_close_write(pipe_t *p) {
	pdirect_cancel_write(p);

	if (p->wrstat != INIT)
		panicstat(p);
	p->wrstat = CLOSING_WRITE;

	debug("p=%p", p);
	p->close_write.im.data = p;
	p->close_write.im.cb = close_write;
	set_immediate(luv_loop(p), &p->close_write.im);
}

void pdirect_cancel_read(pipe_t *p) {
	debugstat(p);
	if (p->rdstat == READING) {
		cancel_immediate(&p->read.im_direct);
		p->rdstat = INIT;
	}
}

void pdirect_cancel_write(pipe_t *p) {
	debugstat(p);
	if (p->wrstat == WRITING) {
		cancel_immediate(&p->write.im_direct);
		p->wrstat = INIT;
	}
	if (p->stat == WRITING) {
		p->stat = INIT;
		pipebuf_unref(p->direct.pool);
	}
}

static int luv_pdirect(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	p->type = PDIRECT_SINK;
	return 1;
}

void luv_pdirect_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pdirect", luv_pdirect);
}

