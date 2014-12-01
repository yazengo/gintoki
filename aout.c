
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

static void deinit(pipe_t *p) {
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

	debug("start n=%d", pb->len);
	aoutdev_write(ao->dev, pb->base, pb->len);
	debug("end   n=%d", pb->len);
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

	if (pb->len & 0x3)
		panic("pb.len=%d invalid. must align 4", pb->len);

	aout_t *ao = (aout_t *)p->read.data;
	ao->pb = pb;
	ao->w.data = p;
	uv_queue_work(luv_loop(p), &ao->w, play_thread, play_done);
}

int luv_aout(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = pipe_new(L, loop);

	aout_t *ao = (aout_t *)zalloc(sizeof(aout_t));
	ao->dev = aoutdev_new();

	p->read.data = ao;
	p->type = PDIRECT_SINK;
	p->gc = deinit;

	pipe_read(p, read_done);

	return 1;
}

void luv_aout_init(lua_State *L, uv_loop_t *loop) {
	aoutdev_init();
	luv_register(L, loop, "aout", luv_aout);
}

