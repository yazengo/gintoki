
#include "luv.h"
#include "pipe.h"
#include "uvwrite.h"

enum {
	IO          = (1<<0),
	IODONE      = (1<<1),
	PAUSED      = (1<<2),
	STOPPING    = (1<<3),
	CLOSE       = (1<<4),
	CLOSING     = (1<<5),
	CLOSED      = (1<<6),
};

static uv_buf_t uv_allocbuf(uv_handle_t *h, size_t n) {
	pipe_t *p = (pipe_t *)h->data;
	return p->read.allocbuf(p, n);
}

static void readfailed(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;
	uv_buf_t ub = {.len = -1};
	p->read.done(p, ub);
	p->flags &= ~IO;
}

static void writefailed(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;
	p->write.done(p, -1);
	p->flags &= ~IO;
}

static void uv_readdone(uv_stream_t *st, ssize_t n, uv_buf_t ub) {
	uv_read_stop(st);
	pipe_t *p = (pipe_t *)st->data;
	ub.len = n;

	if (n < 0)
		p->flags |= RWFAILED;

	if (p->flags & READING) {
		p->read.done(p, ub);
		p->flags &= ~READING;
	}
}

void pstream_read(pipe_t *p, pipe_allocbuf_cb allocbuf, pipe_read_cb done) {
	p->read.allocbuf = allocbuf;
	p->read.done = done;
	p->st->data = p;

	if (p->flags & IO) 
		panic("call read() twice");
	p->flags |= IO;

	if (p->flags & STOPPING) {
		immediate_t *im = &p->read.im;
		im->data = p;
		im->cb = readfailed;
		set_immediate(im);
	} else {
		uv_read_start(p->st, uv_allocbuf, uv_readdone);
	}
}

static void uv_writedone(uv_write_t *w, int stat) {
	pipe_t *p = (pipe_t *)w->data;

	if (stat < 0)
		p->flags |= RWFAILED;
	
	if (p->flags & WRITING) {
		p->write.done(p, stat);
		p->flags &= ~WRITING;
	}
}

void pstream_write(pipe_t *p, uv_buf_t ub, pipe_write_cb done) {
	p->write.done = done;

	if (p->flags & (RWFAILED|FORCESTOP|CLOSING|CLOSED)) {
		immediate_t *im = &p->write.im;
		im->data = p;
		im->cb = writefailed;
		set_immediate(im);
		return;
	} 

	if (p->flags & CANCELLED) {
		// if cancelling write. need not write again
		p->flags &= ~CANCELLED;
		return;
	}

	p->flags |= WRITING;
	p->write.ub = ub;
	p->write.w.data = p;
	uv_write(&p->write.w, p->st, &p->write.ub, 1, uv_writedone);
}

void pstream_forcestop(pipe_t *p) {
	if (p->flags & FORCESTOP)
		panic("dont call forcestop() twice");

	p->flags |= FORCESTOP;
}

static void uv_closed(uv_handle_t *h) {
	pipe_t *p = (pipe_t *)h->data;

	p->flags &= ~CLOSING;
	p->flags |= CLOSED;

	luv_unref(p);
}

void pstream_close(pipe_t *p) {
	if (p->flags & CLOSING)
		panic("dont call close() twice");
	p->flags |= CLOSING;
	p->st->data = p;
	uv_close((uv_handle_t *)p->st, uv_closed);
}

void pstream_cancel_read(pipe_t *p) {
	p->flags &= ~READING;
	uv_read_stop(p->st);
}

void pstream_cancel_write(pipe_t *p) {
	if (p->flags & WRITING) {
		// pending a cancel
		p->flags |= CANCELLED;
	}
}

