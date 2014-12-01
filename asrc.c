
#include <math.h>
#include <stdlib.h>

#include "luv.h"
#include "pipe.h"

typedef struct {
	int mode;
	pipebuf_t *pb;
	pipe_t *p;
} asrc_t;

enum {
	NOISE,
};

static void generate(asrc_t *a);

static void gc(uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;
	asrc_t *a = (asrc_t *)p->write.data;

	free(a);
}

static void write_done(pipe_t *p, int stat) {
	asrc_t *a = (asrc_t *)p->write.data;
	
	if (stat < 0) {
		pipe_close_write(p);
		return;
	}

	generate(a);
}

static void generate(asrc_t *a) {
	a->pb = pipebuf_new();

	if (a->mode == NOISE) {
		void *buf = a->pb->base;
		int len = PIPEBUF_SIZE;

		while (len > 0) {
			*(uint16_t *)buf = rand()&0x7fff;
			buf += 2;
			len -= 2;
		}
	}

	pipe_write(a->p, a->pb, write_done);
}

static int asrc_new(lua_State *L, uv_loop_t *loop) {
	asrc_t *a = (asrc_t *)zalloc(sizeof(asrc_t));
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));

	luv_setgc(p, gc);

	char *mode = (char *)lua_tostring(L, 1);
	if (mode && !strcmp(mode, "noise"))
		a->mode = NOISE;
	a->p = p;

	p->type = PDIRECT_SRC;
	p->read.mode = PREAD_BLOCK;
	p->write.data = a;

	generate(a);

	return 1;
}

void luv_asrc_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "asrc_new", asrc_new);
}

