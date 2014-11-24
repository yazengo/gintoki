
#include <string.h>

#include "luv.h"
#include "pipe.h"
#include "uvwrite.h"

enum {
	INIT,
	READING,
	WRITING,
	CLOSING,
	CLOSED,
};

static uv_buf_t uv_allocbuf(uv_handle_t *h, size_t n) {
	pipe_t *p = (pipe_t *)h->data;

	pipebuf_t *pb = pipebuf_new();
	p->read.pb = pb;
	debug("allocbuf p=%p", pb);

	uv_buf_t ub = {
		.base = pb->base,
		.len = PIPEBUF_SIZE,
	};
	return ub;
}

static void uv_readdone(uv_stream_t *st, ssize_t n, uv_buf_t ub) {
	pipe_t *p = (pipe_t *)st->data;

	uv_read_stop(st);

	if (n == 0)
		panic("n == 0");

	if (p->stat != READING) 
		panic("stat=%d invalid", p->stat);
	p->stat = INIT;

	pipebuf_t *pb = p->read.pb;

	if (n < 0) {
		pipebuf_unref(pb);
		p->read.done(p, NULL);
		return;
	}

	if (n < PIPEBUF_SIZE)
		memset(pb->base + n, 0, PIPEBUF_SIZE - n);
	p->read.done(p, pb);
	p->read.pb = NULL;
}

void pstream_read(pipe_t *p) {
	if (p->stat != INIT)
		panic("stat=%d invalid", p->stat);
	p->stat = READING;

	p->st->data = p;
	uv_read_start(p->st, uv_allocbuf, uv_readdone);
}

static void uv_writedone(uv_write_adv_t *w, int stat) {
	pipe_t *p = (pipe_t *)w->data;

	if (p->stat != WRITING)
		panic("stat=%d invalid", p->stat);
	p->stat = INIT;
	
	p->write.done(p, stat);
}

void pstream_write(pipe_t *p) {
	if (p->stat != INIT)
		panic("stat=%d invalid", p->stat);
	p->stat = WRITING;

	p->write.w.data = p;
	p->write.w.st = p->st;
	uv_write_adv(&p->write.w, p->write.pb, uv_writedone);
}

static void cancel_read(pipe_t *p) {
	uv_read_stop(p->st);

	if (p->read.pb) {
		pipebuf_unref(p->read.pb);
		p->read.pb = NULL;
	}
}

static void cancel_write(pipe_t *p) {
	uv_write_adv_cancel(&p->write.w);
}

void pstream_cancel_read(pipe_t *p) {
	debug("cancel");
	if (p->stat == READING) {
		cancel_read(p);
		p->stat = INIT;
	}
}

void pstream_cancel_write(pipe_t *p) {
	debug("cancel");
	if (p->stat == WRITING) {
		cancel_write(p);
		p->stat = INIT;
	}
}

static void uv_closed(uv_handle_t *h) {
	pipe_t *p = (pipe_t *)h->data;
	p->stat = CLOSED;
	luv_unref(p);
}

static void do_close(pipe_t *p) {
	p->stat = CLOSING;
	p->st->data = p;
	uv_close((uv_handle_t *)p->st, uv_closed);
}

void pstream_close(pipe_t *p) {
	debug("close p=%p", p);

	switch (p->stat) {
	case READING:
		cancel_read(p);
		do_close(p);
		break;

	case WRITING:
		cancel_write(p);
		do_close(p);
		break;

	case INIT:
		do_close(p);
		break;

	default:
		panic("stat=%d invalid", p->stat);
	}
}

