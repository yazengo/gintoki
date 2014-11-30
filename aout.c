
#include <stdlib.h>

#include "utils.h"
#include "luv.h"
#include "pipe.h"
#include "pdirect.h"
#include "pipebuf.h"
#include "aout.h"

typedef struct {
	void *dev;
	uv_work_t w;
	pipebuf_t *pb;
} aout_t;

static void deinit(uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;
	aout_t *ao = (aout_t *)p->read.data;

	info("closed");

	aoutdev_close(ao->dev);
	free(ao);
}

static void read_done(pipe_t *p, pipebuf_t *pb);

static void play_thread(uv_work_t *w) {
	pipe_t *p = (pipe_t *)w->data;
	aout_t *ao = (aout_t *)p->read.data;
	pipebuf_t *pb = ao->pb;

	debug("n=%d", PIPEBUF_SIZE);
	aoutdev_play(ao->dev, pb->base, PIPEBUF_SIZE);
	debug("n=%d", PIPEBUF_SIZE);
}

static void play_done(uv_work_t *w, int stat) {
	pipe_t *p = (pipe_t *)w->data;
	aout_t *ao = (aout_t *)p->read.data;

	pipebuf_unref(ao->pb);
	pipe_read(p, read_done);
}

static void read_done(pipe_t *p, pipebuf_t *pb) {
	if (pb == NULL) {
		debug("close");
		pipe_close_read(p);
		return;
	}

	int n = PIPEBUF_SIZE;
	debug("n=%d", n);

	aout_t *ao = (aout_t *)p->read.data;
	ao->pb = pb;
	ao->w.data = p;
	uv_queue_work(luv_loop(p), &ao->w, play_thread, play_done);
}

int luv_aout(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	aout_t *ao = (aout_t *)zalloc(sizeof(aout_t));

	p->read.data = ao;
	p->type = PDIRECT_SINK;

	ao->dev = aoutdev_new();
	luv_setgc(p, deinit);

	pipe_read(p, read_done);

	return 1;
}

void luv_aout_init(lua_State *L, uv_loop_t *loop) {
	ao_initialize();

	luv_register(L, loop, "aout", luv_aout);
}

