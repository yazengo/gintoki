
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
	CLOSED_WRITE,
	CLOSED_READ,
	CLOSED,
};

static void do_read(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;
	debug("read stat=%d type=%d", p->stat, p->type);

	switch (p->stat) {
	case WRITING:
		p->write.done(p, 0);
		p->read.done(p, p->direct.pool);
		p->stat = INIT;
		break;

	case INIT:
		p->stat = READING;
		break;
	
	case CLOSED_WRITE:
		p->read.done(p, NULL);
		break;

	default:
		panic("stat=%d type=%d invalid", p->stat, p->type);
	}
}

void pdirect_read(pipe_t *p) {
	debug("read");
	p->read.im_direct.data = p;
	p->read.im_direct.cb = do_read;
	set_immediate(luv_loop(p), &p->read.im_direct);
}

static void do_write(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;
	debug("write stat=%d", p->stat);

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
		panic("stat=%d invalid", p->stat);
	}
}

void pdirect_write(pipe_t *p) {
	debug("write");
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

	debug("close stat=%d", p->stat);

	switch (p->stat) {
	case INIT:
		p->stat = CLOSED_READ;
		break;

	case READING:
		p->stat = CLOSED_READ;
		// need not call read.done
		break;

	case WRITING:
		pipebuf_unref(p->direct.pool);
		p->write.done(p, -1);
		p->stat = CLOSED_READ;
		break;

	case CLOSED_WRITE:
		do_close(p);
		break;

	default:
		panic("stat=%d invalid", p->stat);
	}
}

void pdirect_close_read(pipe_t *p) {
	debug("close");
	p->close_read.im.data = p;
	p->close_read.im.cb = close_read;
	set_immediate(luv_loop(p), &p->close_read.im);
}

static void close_write(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;

	switch (p->stat) {
	case INIT:
		p->stat = CLOSED_WRITE;
		break;

	case READING:
		p->read.done(p, NULL);
		p->stat = CLOSED_WRITE;
		break;

	case WRITING:
		pipebuf_unref(p->direct.pool);
		p->stat = CLOSED_WRITE;
		break;

	case CLOSED_READ:
		do_close(p);
		break;

	default:
		panic("stat=%d invalid", p->stat);
	}
}

void pdirect_close_write(pipe_t *p) {
	debug("close");
	p->close_write.im.data = p;
	p->close_write.im.cb = close_write;
	set_immediate(luv_loop(p), &p->close_write.im);
}

void pdirect_cancel_read(pipe_t *p) {
	if (p->stat == READING)
		p->stat = INIT;
}

void pdirect_cancel_write(pipe_t *p) {
	if (p->stat == WRITING) {
		pipebuf_unref(p->direct.pool);
		p->stat = INIT;
	}
}

