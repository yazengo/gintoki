
#include "luv.h"
#include "utils.h"
#include "pdirect.h"

enum {
	DOING_R   = 0x1,
	DOING_W   = 0x2,
	PENDING_R = 0x4,
	PENDING_W = 0x8,
	AGAIN_R   = 0x10,
	AGAIN_W   = 0x20,
	CLOSED_R  = 0x40,
	CLOSED_W  = 0x80,
	STOP      = 0x100,
	CLOSE     = 0x200,
	PAUSE     = 0x400,
	CANCEL_R  = 0x800,
	CANCEL_W  = 0x1000,
};

static void do_read(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;
	p->flags &= ~DOING_R;

	if (p->flags & CANCEL_R)
		return;
	if (p->flags & PAUSE) {
		p->flags |= AGAIN_R;
		return;
	}
	if (p->flags & STOP) {
		p->flags |= CLOSED_R;
		p->read.ub.len = -1;
	}
	p->read.done(p, p->read.ub);
}

static void do_write(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;
	p->flags &= ~DOING_W;

	if (p->flags & CANCEL_W)
		return;
	if (p->flags & PAUSE) {
		p->flags |= AGAIN_W;
		return;
	}
	if (p->flags & STOP) {
		p->flags |= CLOSED_W;
		p->write.stat = -1;
	}
	p->write.done(p, p->write.stat);
}

void pdirect_read(pipe_t *p, int len, pipe_read_cb done) {
	if (p->flags & (PENDING_R | DOING_R | AGAIN_R))
		panic("dont call read() twice");
	if (len == 0)
		panic("len must > 0");

	p->read.done = done;
	p->read.len = len;

	if (len > p->write.ub.len)
		len = p->write.ub.len;
	p->read.ub.base = p->write.ub.base;
	p->read.ub.len = len;

	p->flags |= DOING_R;
	immediate_t *im = &p->read.im;
	im->data = p;
	im->cb = do_read;
	set_immediate(im);

	p->write.ub.base += len;
	p->write.ub.len -= len;
	if (p->write.ub.len == 0) {
		p->flags &= ~PENDING_W;
		p->flags |= DOING_W;

		p->write.stat = 0;
		immediate_t *im = &p->write.im;
		im->data = p;
		im->cb = do_write;
		set_immediate(im);
	}
}

void pdirect_write(pipe_t *p, uv_buf_t ub, pipe_write_cb done) {
	if (p->flags & (PENDING_R | DOING_R | AGAIN_R))
		panic("dont call read() twice");
	if (len == 0)
		panic("len must > 0");

	p->read.done = done;

	if (!(p->flags & PENDING_W)) {
		p->flags |= PENDING_R;
		return;
	}

	if (len > p->write.ub.len)
		len = p->write.ub.len;
	p->read.ub.base = p->write.ub.base;
	p->read.ub.len = len;

	p->flags |= DOING_R;
	immediate_t *im = &p->read.im;
	im->data = p;
	im->cb = do_read;
	set_immediate(im);

	p->write.ub.base += len;
	p->write.ub.len -= len;
	if (p->write.ub.len == 0) {
		p->flags &= ~PENDING_W;
		p->flags |= DOING_W;

		p->write.stat = 0;
		immediate_t *im = &p->write.im;
		im->data = p;
		im->cb = do_write;
		set_immediate(im);
	}

}

void pdirect_stop(pipe_t *p) {
	if (p->flags & STOP)
		panic("dont call stop() twice");
	p->flags |= STOP;
}

static void do_close(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;
}

void pdirect_close(pipe_t *p) {
	if (p->flags & CLOSING)
		panic("dont call close() twice");
	p->flags |= CLOSING;

	immediate_t *im = &p->close.im;
	im->data = p;
	im->cb = do_close;
	set_immediate(im);
}

void pdirect_cancel_read(pipe_t *p) {
	if (p->flags & READING)
		p->flags &= ~READING;
}

void pdirect_cancel_write(pipe_t *p) {
	if (p->flags & WRITING)
		p->flags &= ~WRITING;
}

