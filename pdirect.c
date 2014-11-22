
#include <string.h>
#include <stdlib.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"
#include "pdirect.h"

enum {
	PS_PENDING_R = PS_MAX<<1,
	PS_PENDING_W = PS_MAX<<2,
};

static void do_trans(pipe_t *p) {
	uv_buf_t *pool = &p->direct.pool;
	uv_buf_t ub;

	if (p->read.allocbuf) {
		ub = p->read.allocbuf(p, 1024);
		if (ub.len > pool->len)
			ub.len = pool->len;
		memcpy(ub.base, pool->base, ub.len);
	} else {
		ub = *pool;
	}

	pool->base += ub.len;
	pool->len -= ub.len;

	if (pool->len == 0) {
		p->stat &= ~PS_PENDING_W;
		p->write.done(p, 0);
	}

	p->read.done(p, ub.len, ub);
	p->stat &= ~PS_PENDING_R;
}

static void do_read(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;

	if (!(p->stat & PS_PENDING_W)) {
		p->stat |= PS_PENDING_R;
		return;
	}

	do_trans(p);
}

void pdirect_read(pipe_t *p) {
	p->read.im_direct.data = p;
	p->read.im_direct.cb = do_read;
	set_immediate(luv_loop(p), &p->read.im_direct);
}

static void do_write(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;

	if (!(p->stat & PS_PENDING_R)) {
		p->stat |= PS_PENDING_W;
		return;
	}

	do_trans(p);
}

void pdirect_write(pipe_t *p) {
	p->read.im_direct.data = p;
	p->read.im_direct.cb = do_write;
	set_immediate(luv_loop(p), &p->read.im_direct);
}

void pdirect_cancel_read(pipe_t *p) {
	cancel_immediate(&p->read.im_direct);
	p->stat &= ~PS_PENDING_R;
}

void pdirect_cancel_write(pipe_t *p) {
	cancel_immediate(&p->write.im_direct);
	p->stat &= ~PS_PENDING_W;
}

void pdirect_close(pipe_t *p) {
	luv_unref(p);
}

