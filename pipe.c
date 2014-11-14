
#include <string.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"
#include "pdirect.h"

#define BUF_SIZE 1024

uv_buf_t pipe_allocbuf(pipe_t *p, int n) {
	if (p->read.buf == NULL)
		p->read.buf = zalloc(BUF_SIZE);
	return uv_buf_init(p->read.buf, BUF_SIZE);
}

static void pdirect_readdone(pipe_t *p, uv_buf_t ub) {
	if (ub.len < 0) {
		p->read.done(p, ub);
		return;
	}

	uv_buf_t to = p->read.to;
	if (to.len > ub.len)
		to.len = ub.len;
	memcpy(to.base, ub.base, to.len);
	p->read.done(p, to);
}

void pipe_read(pipe_t *p, pipe_allocbuf_cb allocbuf, pipe_read_cb done) {
	p->read.done = done;

	if (p->type == PT_DIRECT) {
		if (allocbuf) {
			uv_buf_t to = allocbuf(p, BUF_SIZE);
			p->read.to = to;
			pdirect_read(p, to.len, pdirect_readdone);
		} else {
			pdirect_read(p, -1, done);
		}
	} else if (p->type == PT_STREAM) {
		pstream_read(p, allocbuf, done);
	}
}

void pipe_write(pipe_t *p, uv_buf_t ub, pipe_write_cb done) {
	if (p->type == PT_DIRECT) {
		pdirect_write(p, ub, done);
	} else if (p->type == PT_STREAM) {
		pstream_write(p, ub, done);
	}
}

void pipe_close(pipe_t *p) {
	if (p->type == PT_DIRECT) {
		pdirect_close(p);
	} else if (p->type == PT_STREAM) {
		pstream_close(p);
	}
}

void pipe_cancel_read(pipe_t *p) {
	if (p->type == PT_DIRECT) {
		pdirect_cancel_read(p);
	} else if (p->type == PT_STREAM) {
		pstream_cancel_read(p);
	}
}

void pipe_cancel_write(pipe_t *p) {
	if (p->type == PT_DIRECT) {
		pdirect_cancel_write(p);
	} else if (p->type == PT_STREAM) {
		pstream_cancel_write(p);
	}
}

void pipe_forcestop(pipe_t *p) {
	if (p->type == PT_DIRECT) {
		pdirect_forcestop(p);
	} else if (p->type == PT_STREAM) {
		pstream_forcestop(p);
	}
}

