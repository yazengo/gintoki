
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

	pipebuf_unref(wa->pb);

	if (wa->stat == WRITING) {
		wa->stat = INIT;
		wa->done(wa, stat);
	} else if (wa->stat == WRITING_CANCEL) {
		wa->stat = INIT;
	} else if (wa->stat == WRITING_PENDING) {
		wa->stat = WRITING;
		wa->pb = wa->pb_pending;

		wa->ub.base = wa->pb->base;
		wa->ub.len = wa->pb->len;
		uv_write(&wa->w, wa->st, &wa->ub, 1, write_done);
	}
}

void uv_write_adv_cancel(uv_write_adv_t *wa) {
	if (wa->stat == WRITING) {
		wa->stat = WRITING_CANCEL;
	} else if (wa->stat == WRITING_PENDING) {
		wa->stat = WRITING_CANCEL;
		pipebuf_unref(wa->pb);
	}
}

void uv_write_adv(uv_write_adv_t *wa, pipebuf_t *pb, uv_write_adv_cb done) {
	if (wa->stat == INIT) {
		// can read
		wa->stat = WRITING;
		wa->w.data = wa;
		wa->done = done;
		wa->pb = pb;

		wa->ub.base = wa->pb->base;
		wa->ub.len = wa->pb->len;
		uv_write(&wa->w, wa->st, &wa->ub, 1, write_done);
	} else if (wa->stat == WRITING_CANCEL) {
		// pending
		wa->stat = WRITING_PENDING;
		wa->pb_pending = pb;
	} else {
		panic("dont call write() twice");
	}
}

void uv_write_adv_close(uv_write_adv_t *wa, uv_close_cb done) {
	uv_close((uv_handle_t *)wa->st, done);
}

