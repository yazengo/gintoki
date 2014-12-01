
#include "pcm.h"
#include "luv.h"
#include "pipe.h"

typedef struct {
	int stat;
	float vol;
	float fadedur;
	float fadepos;
	pipe_t *src;
	pipe_t *sink;
} aeffect_t;

enum {
	INIT,
	FADEIN,
	FADEOUT,
};

static void read_done(pipe_t *p, pipebuf_t *pb);

static void process(aeffect_t *a, void *buf, int len) {
	switch (a->stat) {
	case FADEOUT:
	case FADEIN: 
	{
		float dur = (float)len / (44100*4.0);

		if (a->fadepos > a->fadedur) {
			if (a->stat == FADEIN)
				pcm_do_volume(buf, len, a->vol);
			else
				pcm_do_volume(buf, len, 0.0);
			return;
		}
		a->fadepos += dur;

		float per;
		if (a->stat == FADEIN)
			per = a->fadepos/a->fadedur;
		else
			per = 1.0 - a->fadepos/a->fadedur;

		debug("fade pos=%lf/%lf", a->fadepos, a->fadedur);
		float vol = a->vol * per;
		pcm_do_volume(buf, len, vol);
		break;
	}

	case INIT:
		if (a->vol != 1.0) {
			pcm_do_volume(buf, len, a->vol);
			debug("buf=%p len=%d vol=%lf", buf, len, a->vol);
		}
		break;
	}
}

static void close_all(aeffect_t *a) {
	pipe_close_read(a->src);
	pipe_close_write(a->sink);
}

static void write_done(pipe_t *p, int stat) {
	aeffect_t *a = (aeffect_t *)p->data;

	if (stat < 0) {
		close_all(a);
		return;
	}

	pipe_read(a->src, read_done);
}

static void read_done(pipe_t *p, pipebuf_t *pb) {
	aeffect_t *a = (aeffect_t *)p->data;
	
	debug("p=%p", p);

	if (pb == NULL) {
		close_all(a);
		return;
	}

	process(a, pb->base, pb->len);

	pipe_write(a->sink, pb, write_done);
}

static int aeffect_setopt(lua_State *L, uv_loop_t *loop) {
	aeffect_t *a = (aeffect_t *)luv_toctx(L, 1);
	char *op = (char *)lua_tostring(L, 2);

	if (!strcmp(op, "setvol")) {
		a->vol = lua_tonumber(L, 3);
		info("vol=%lf", a->vol);
		return 0;
	}

	if (!strcmp(op, "getvol")) {
		lua_pushnumber(L, a->vol);
		return 1;
	}

	if (!strcmp(op, "cancel_fade")) {
		a->stat = INIT;
		return 0;
	}

	if (!strcmp(op, "fadein")) {
		a->stat = FADEIN;
		a->fadedur = lua_tonumber(L, 3)/1e3;
		a->fadepos = 0.0;
		info("fadein dur=%lf", a->fadedur);
		return 0;
	}

	if (!strcmp(op, "fadeout")) {
		a->stat = FADEOUT;
		a->fadedur = lua_tonumber(L, 3)/1e3;
		a->fadepos = 0.0;
		info("fadeout dur=%lf", a->fadedur);
		return 0;
	}

	return 0;
}

static int aeffect_new(lua_State *L, uv_loop_t *loop) {
	aeffect_t *a = (aeffect_t *)luv_newctx(L, loop, sizeof(aeffect_t));
	a->vol = 1.0;

	pipe_t *src = pipe_new(L, loop);
	src->data = a;
	src->type = PDIRECT_SINK;

	pipe_t *sink = pipe_new(L, loop);
	sink->data = a;
	sink->type = PDIRECT_SRC;

	a->src = src;
	a->sink = sink;

	pipe_read(a->src, read_done);

	return 3;
}

void luv_aeffect_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "aeffect_new", aeffect_new);
	luv_register(L, loop, "aeffect_setopt", aeffect_setopt);
}

