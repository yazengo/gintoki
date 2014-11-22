
#include "luv.h"
#include "pipe.h"
#include "uvwrite.h"

static uv_buf_t uv_allocbuf(uv_handle_t *h, size_t n) {
	pipe_t *p = (pipe_t *)h->data;
	pipe_allocbuf_cb allocbuf = p->read.allocbuf;
	if (allocbuf == NULL)
		allocbuf = pipe_allocbuf;
	return allocbuf(p, n);
}

static void uv_readdone(uv_stream_t *st, ssize_t n, uv_buf_t ub) {
	pipe_t *p = (pipe_t *)st->data;

	if (n == 0)
		panic("n == 0");

	if (n > 0)
		ub.len = n;
	uv_read_stop(st);
	p->read.done(p, n, ub);
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
	uv_write_adv(&p->write.w, p->write.ub, uv_writedone);
}

static void uv_closed(uv_handle_t *h) {
	pipe_t *p = (pipe_t *)h->data;
	luv_unref(p);
}

void pstream_close(pipe_t *p) {
	p->st->data = p;
	uv_close((uv_handle_t *)p->st, uv_closed);
}

void pstream_cancel_read(pipe_t *p) {
	uv_read_stop(p->st);
}

void pstream_cancel_write(pipe_t *p) {
	p->write.w.data = p;
	p->write.w.st = p->st;
	uv_write_adv_cancel(&p->write.w);
}

