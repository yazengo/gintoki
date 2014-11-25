
#include <math.h>
#include <stdlib.h>

#include "luv.h"
#include "pipe.h"

typedef struct {
	int mode;
	int stat;
	pipebuf_t *pb;
	pipe_t *p;
} asrc_t;

enum {
	NOISE,
};

enum {
	INIT,
	WRITING,
	PAUSED,
	CLOSED,
};

static void generate(asrc_t *a);

static void gc(uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;
	asrc_t *a = (asrc_t *)p->write.data;

	free(a);
}

static void pause(asrc_t *a) {
	if (a->stat == WRITING)
		pipe_cancel_write(a->p);
	a->stat = PAUSED;
}

static void resume(asrc_t *a) {
	if (a->stat != PAUSED)
		return;
	generate(a);
}


static int asrc_setopt(lua_State *L, uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;
	asrc_t *a = (asrc_t *)p->write.data;
	char *op = (char *)lua_tostring(L, 1);

	if (op && !strcmp(op, "pause")) {
		pause(a);
		return 0;
	}

	if (op && !strcmp(op, "resume")) {
		resume(a);
		return 0;
	}

	return 0;
}

static void write_done(pipe_t *p, int stat) {
	asrc_t *a = (asrc_t *)p->write.data;
	
	if (stat < 0) {
		a->stat = CLOSED;
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

	a->stat = WRITING;
	pipe_write(a->p, a->pb, write_done);
}

static int asrc(lua_State *L, uv_loop_t *loop) {
	asrc_t *a = (asrc_t *)zalloc(sizeof(asrc_t));
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));

	luv_pushcclosure(L, asrc_setopt, p);
	lua_setfield(L, -2, "setopt");

	luv_setgc(p, gc);

	char *mode = (char *)lua_tostring(L, 1);
	if (mode && !strcmp(mode, "noise"))
		a->mode = NOISE;
	a->p = p;

	p->type = PDIRECT_SRC;
	p->write.data = a;

	generate(a);

	return 1;
}

void luv_asrc_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "asrc", asrc);
}

