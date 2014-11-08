
#include <stdlib.h>

#include "luv.h"
#include "utils.h"

static void on_timeout(uv_timer_t *t, int stat) {
	lua_State *L = luv_state(t);

	luv_pushctx(L, t);
	lua_getfield(L, -1, "done");
	lua_call_or_die(L, 0, 0);
	lua_pop(L, 1);

	if (uv_timer_get_repeat(t) == 0) {
		luv_unref(t);
	}
}

static void timer_gc(uv_loop_t *loop, void *_t) {
	uv_timer_t *t = (uv_timer_t *)_t;
	uv_close((uv_handle_t *)t, NULL);
}

static int set_timer(lua_State *L, uv_loop_t *loop, int repeat) {
	int timeout = lua_tonumber(L, 2);

	uv_timer_t *t = (uv_timer_t *)luv_newctx(L, loop, sizeof(uv_timer_t));
	uv_timer_init(loop, t);
	uv_timer_start(t, on_timeout, timeout, repeat ? timeout : 0);

	luv_setgc(t, timer_gc);

	lua_pushvalue(L, 1);
	lua_setfield(L, -2, "done");

	return 1;
}

static int set_timeout(lua_State *L, uv_loop_t *loop) {
	return set_timer(L, loop, 0);
}

static int set_interval(lua_State *L, uv_loop_t *loop) {
	return set_timer(L, loop, 1);
}

static int clear_timer(lua_State *L, uv_loop_t *loop) {
	uv_timer_t *t = (uv_timer_t *)luv_toctx(L, 1);
	uv_timer_stop(t);
	luv_unref(t);
	return 0;
}

void luv_timer_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "set_timeout", set_timeout);
	luv_register(L, loop, "set_interval", set_interval);
	luv_register(L, loop, "clear_timeout", clear_timer);
	luv_register(L, loop, "clear_interval", clear_timer);
}

