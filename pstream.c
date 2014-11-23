
#include "luv.h"
#include "pipe.h"
#include "pipebuf.h"
#include "uvwrite.h"

enum {
	PS_PENDING_R = PS_MAX<<1,
};

static uv_buf_t uv_allocbuf(uv_handle_t *h, size_t n) {
	pipe_t *p = (pipe_t *)h->data;

	pipebuf_t *pb = pipebuf_new();
	p->read.pb = pb;
	p->stat |= PS_PENDING_R;

	uv_buf_t ub = {
		.base = pb->base,
		.len = pb->len,
	};
	return ub;
}

static void uv_readdone(uv_stream_t *st, ssize_t n, uv_buf_t ub) {
	pipe_t *p = (pipe_t *)st->data;

	uv_read_stop(st);
	p->stat &= ~PS_PENDING_R;

	if (n == 0)
		panic("n == 0");

	if (n < 0) {
		p->read.done(p, NULL);
	} else {
		p->read.pb->len = n;
		p->read.done(p, p->read.pb);
	}
}

void pstream_read(pipe_t *p) {
	p->st->data = p;
	uv_read_start(p->st, uv_allocbuf, uv_readdone);
}

static void uv_writedone(uv_write_adv_t *w, int stat) {
	pipe_t *p = (pipe_t *)w->data;
	p->write.done(p, stat);
}

void pstream_write(pipe_t *p) {
	p->write.w.data = p;
	p->write.w.st = p->st;
	uv_write_adv(&p->write.w, p->write.pb, uv_writedone);
}

static void uv_closed(uv_handle_t *h) {
	pipe_t *p = (pipe_t *)h->data;
	luv_unref(p);
}

void pstream_close(pipe_t *p) {
	debug("close p=%p", p);
	p->st->data = p;
	uv_close((uv_handle_t *)p->st, uv_closed);
}

void pstream_cancel_read(pipe_t *p) {
	uv_read_stop(p->st);

	if (p->stat & PS_PENDING_R) {
		pipebuf_unref(p->read.pb);
		p->stat &= ~PS_PENDING_R;
	}
}

void pstream_cancel_write(pipe_t *p) {
	p->write.w.data = p;
	p->write.w.st = p->st;
	uv_write_adv_cancel(&p->write.w);
}

