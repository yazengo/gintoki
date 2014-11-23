
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

static void do_read(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;

	if (!(p->stat & PS_PENDING_W)) {
		p->stat |= PS_PENDING_R;
		debug("pending");
		return;
	}

	p->stat &= ~PS_PENDING_W;
	p->write.done(p, 0);
	p->read.done(p, p->direct.pool);
}

void pdirect_read(pipe_t *p) {
	debug("read");
	p->read.im_direct.data = p;
	p->read.im_direct.cb = do_read;
	set_immediate(luv_loop(p), &p->read.im_direct);
}

static void do_write(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;
	
	p->direct.pool = p->write.pb;

	if (!(p->stat & PS_PENDING_R)) {
		p->stat |= PS_PENDING_W;
		debug("pending");
		return;
	}

	p->stat &= ~PS_PENDING_R;
	p->read.done(p, p->direct.pool);
	p->write.done(p, 0);
}

void pdirect_write(pipe_t *p) {
	p->write.im_direct.data = p;
	p->write.im_direct.cb = do_write;
	set_immediate(luv_loop(p), &p->write.im_direct);
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

