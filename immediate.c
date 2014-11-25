
#include "utils.h"
#include "luv.h"
#include "prof.h"
#include "mem.h"
#include "immediate.h"

static objpool_t op_async = {
	.name = "uv_async_t",
	.size = sizeof(uv_async_t),
};
prof_t pf_immediate = {"immediate"};

#define USE_QUEUE

static queue_t immque;

void run_immediate() {
	queue_t *q;
	queue_foreach(q, &immque) {
		immediate_t *im = queue_data(q, immediate_t, q);
		queue_remove(&im->q);

		im->cb(im);
		im->cb = NULL;
	}
}

static void immediate_closed_async(uv_handle_t *h) {
	objpool_put(&op_async, h);
}

static void immediate_cb_async(uv_async_t *a, int stat) {
	immediate_t *im = (immediate_t *)a->data;
	uv_close((uv_handle_t *)a, immediate_closed_async);
	im->a = NULL;
	im->cb(im);
}

static void cancel_immediate_async(immediate_t *im) {
	uv_close((uv_handle_t *)im->a, immediate_closed_async);
	im->a = NULL;
}

void set_immediate_async(uv_loop_t *loop, immediate_t *im) {
	if (im->a)
		panic("don't call twice");
	uv_async_t *a = (uv_async_t *)objpool_get(&op_async);
	a->data = im;
	im->a = a;
	uv_async_init(loop, a, immediate_cb_async);
	uv_async_send(a);
}

static void cancel_immediate_queue(immediate_t *im) {
	if (im->cb) {
		im->cb = NULL;
		queue_remove(&im->q);
	}
}

static void set_immediate_queue(uv_loop_t *loop, immediate_t *im) {
	queue_insert_tail(&immque, &im->q);
}

void set_immediate(uv_loop_t *loop, immediate_t *im) {
	prof_inc(&pf_immediate);
#ifdef USE_QUEUE
	set_immediate_queue(loop, im);
#else
	set_immediate_async(loop, im);
#endif
}

void cancel_immediate(immediate_t *im) {
#ifdef USE_QUEUE
	cancel_immediate_queue(im);
#else
	cancel_immediate_async(im);
#endif
}

static void luv_immediate_cb(immediate_t *im) {
	luv_callfield(im, "done_cb", 0, 0);
	luv_unref(im);
}

static int luv_set_immediate(lua_State *L, uv_loop_t *loop) {
	immediate_t *im = (immediate_t *)luv_newctx(L, loop, sizeof(immediate_t));
	im->cb = luv_immediate_cb;
	lua_pushvalue(L, 1);
	lua_setfield(L, -2, "done_cb");
	set_immediate(loop, im);
	return 1;
}

static int luv_cancel_immediate(lua_State *L, uv_loop_t *loop) {
	immediate_t *im = (immediate_t *)luv_toctx(L, 1);
	cancel_immediate(im);
	return 0;
}

void luv_immediate_init(lua_State *L, uv_loop_t *loop) {
	queue_init(&immque);
	luv_register(L, loop, "set_immediate", luv_set_immediate);
	luv_register(L, loop, "cancel_immediate", luv_cancel_immediate);
}

