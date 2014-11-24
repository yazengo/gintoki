
#include <stdlib.h>

#include "utils.h"
#include "pipebuf.h"
#include "mem.h"

static objpool_t op_pipebuf = {
	.name = "pipebuf",
	.size = PIPEBUF_ALLOCSIZE,
};

static void gc(pipebuf_t *pb) {
	debug("gc p=%p", pb);
	objpool_put(&op_pipebuf, pb);
}

pipebuf_t *pipebuf_new() {
	pipebuf_t *pb = (pipebuf_t *)objpool_get(&op_pipebuf);
	pb->base = pb->buf;
	pb->len = PIPEBUF_SIZE;
	pb->refcnt = 1;
	pb->gc = gc;
	debug("new p=%p", pb);
	return pb;
}

void pipebuf_ref(pipebuf_t *pb) {
	pb->refcnt++;
}

void pipebuf_unref(pipebuf_t *pb) {
	pb->refcnt--;
	if (pb->refcnt == 0 && pb->gc)
		pb->gc(pb);
}

