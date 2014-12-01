
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

static void uv_readdone(uv_stream_t *st, ssize_t n, uv_buf_t ub);

static uv_buf_t uv_allocbuf(uv_handle_t *h, size_t n) {
	pipe_t *p = (pipe_t *)h->data;

	pipebuf_t *pb = p->read.pb;
	int len = p->read.len;

	if (pb == NULL) 
		pb = pipebuf_new();
	
	p->read.pb = pb;
	debug("pb=%p len=%d", pb, len);

	uv_buf_t ub = {
		.base = pb->base + len,
		.len = PIPEBUF_SIZE - len,
	};
	return ub;
}

static void normal_readdone(pipe_t *p, ssize_t n) {
	pipebuf_t *pb = p->read.pb;

	debug("n=%d", n);

	if (n < 0) {
		p->stat = INIT;
		p->read.done(p, NULL);
		pipebuf_unref(pb);
	} else {
		p->stat = INIT;
		pb->len = n;
		p->read.done(p, pb);
	}

	p->read.pb = NULL;
}

static void block_readdone(pipe_t *p, ssize_t n) {
	pipebuf_t *pb = p->read.pb;
	int len = p->read.len;

	debug("pb=%p pb.base=%p n=%d len=%d", pb, pb->base, n, len);

	if (n < 0) {
		if (len > 0) {
			memset(pb->base + len, 0, PIPEBUF_SIZE - len);
			p->stat = INIT;
			p->read.done(p, pb);
			pb = NULL;
			len = 0;
		} else {
			p->stat = INIT;
			p->read.done(p, NULL);
			pipebuf_unref(pb);
			pb = NULL;
			len = 0;
		}
	} else {
		len += n;
		if (len == PIPEBUF_SIZE) {
			p->stat = INIT;
			p->read.done(p, pb);
			pb = NULL;
			len = 0;
		} else {
			p->stat = READING;
			uv_read_start(p->st, uv_allocbuf, uv_readdone);
		}
	}

	p->read.pb = pb;
	p->read.len = len;
}

static void uv_readdone(uv_stream_t *st, ssize_t n, uv_buf_t ub) {
	pipe_t *p = (pipe_t *)st->data;

	uv_read_stop(st);

	if (p->stat != READING) 
		panic("stat=%d invalid", p->stat);

	if (p->read.mode == PREAD_BLOCK) 
		block_readdone(p, n);
	else
		normal_readdone(p, n);
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
	debug("p=%p", p);
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

