
#include <stdlib.h>

#include "utils.h"
#include "pipebuf.h"

#define PIPEBUF_SIZE 2048

static void gc(pipebuf_t *pb) {
	free(pb->base);
	free(pb);
}

pipebuf_t *pipebuf_new() {
	pipebuf_t *pb = (pipebuf_t *)zalloc(sizeof(pipebuf_t));

	pb->base = zalloc(PIPEBUF_SIZE);
	pb->len = PIPEBUF_SIZE;
	pb->refcnt = 1;
	pb->gc = gc;

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

