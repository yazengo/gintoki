
#include <stdlib.h>
#include <stdio.h>

#include "luv.h"
#include "utils.h"
#include "pipebuf.h"
#include "mem.h"

int PIPEBUF_ALLOCSIZE;
int PIPEBUF_SIZE;

static objpool_t op_pipebuf = { .name = "pipebuf" };

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
	debug("new pb=%p", pb);
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

void luv_pipebuf_init(lua_State *L, uv_loop_t *loop) {
	char *s = getenv("PIPEBUF");
	int size = 1024;
	if (s)
		sscanf(s, "%d", &size);
	PIPEBUF_ALLOCSIZE = size - sizeof(obj_t);
	PIPEBUF_SIZE = PIPEBUF_ALLOCSIZE - sizeof(pipebuf_t);
	op_pipebuf.size = PIPEBUF_ALLOCSIZE;
}

