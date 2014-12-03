
#include <stdlib.h>

#include "mem.h"
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
	pipe_t *p;
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

	if (ao->dev == NULL)
		ao->dev = aoutdev_new();

	static float last_tm = 0.0;
	float now_tm = now();

	if (last_tm == 0.0)
		last_tm = now_tm;

	float tm = now_tm;
	debug("start n=%d p=%p delay=%f", pb->len, p, (now_tm-last_tm)*1000);
	last_tm = now_tm;

	aoutdev_write(ao->dev, pb->base, pb->len);

	now_tm = now();
	debug("end   n=%d p=%p tm=%f", pb->len, p, (now_tm-tm)*1000);
}

static void play_done(uv_work_t *w, int stat) {
	pipe_t *p = (pipe_t *)w->data;
	aout_t *ao = (aout_t *)p->read.data;

	debug("p=%p", p);
	pipebuf_unref(ao->pb);
	pipe_read(p, read_done);
}

static void write_done(uv_write_t *wreq, int stat) {
	aout_t *ao = (aout_t *)wreq->data;

	debug("p=%p pb=%p", ao->p, ao->pb);

	pipebuf_unref(ao->pb);
	pipe_read(ao->p, read_done);
}

static void read_done(pipe_t *p, pipebuf_t *pb) {
	aout_t *ao = p->read.data;

	if (pb == NULL) {
		debug("close");
		pipe_close_read(p);
		return;
	}

	if (pb->len & 0x3)
		panic("pb.len=%d invalid. must align 4", pb->len);

	debug("p=%p pb=%p", p, pb);
	ao->pb = pb;
	ao->w.data = p;
	uv_queue_work(luv_loop(p), &ao->w, play_thread, play_done);
}

int luv_aout(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = pipe_new(L, loop);

	aout_t *ao = (aout_t *)zalloc(sizeof(aout_t));
	ao->p = p;

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

