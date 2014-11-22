
#include "luv.h"
#include "uvwrite.h"
#include "utils.h"

enum {
	INIT,
	WRITING,
	WRITING_CANCEL,
	WRITING_PENDING,
};

static void write_done(uv_write_t *w, int stat) {
	uv_write_adv_t *wa = (uv_write_adv_t *)w->data;

	if (wa->stat == WRITING) {
		wa->stat = INIT;
		wa->done(wa, stat);
	} else if (wa->stat == WRITING_CANCEL) {
		wa->stat = INIT;
	} else if (wa->stat == WRITING_PENDING) {
		wa->stat = WRITING;
		uv_write(&wa->w, wa->st, &wa->ub_pending, 1, write_done);
	}
}

void uv_write_adv_cancel(uv_write_adv_t *wa) {
	if (wa->stat == WRITING) {
		wa->stat = WRITING_CANCEL;
	} else if (wa->stat == WRITING_CANCEL) {
		wa->stat = WRITING;
	} else if (wa->stat == WRITING_PENDING) {
		wa->stat = WRITING_CANCEL;
	}
}

void uv_write_adv(uv_write_adv_t *wa, uv_buf_t ub, uv_write_adv_cb done) {
	if (wa->stat == INIT) {
		// can read
		wa->stat = WRITING;
		wa->w.data = wa;
		wa->done = done;
		wa->ub = ub;
		debug("p=%p n=%d", wa->ub.base, wa->ub.len);
		uv_write(&wa->w, wa->st, &wa->ub, 1, write_done);
	} else if (wa->stat == WRITING_CANCEL) {
		// pending
		wa->stat = WRITING_PENDING;
		wa->ub_pending = ub;
	} else {
		panic("dont call write() twice");
	}
}

