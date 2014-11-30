
#include <stdlib.h>

#include "luv.h"
#include "utils.h"

static void timer_closed(uv_handle_t *t) {
	luv_unref(t);
}

static void timer_close(uv_timer_t *t) {
	uv_close((uv_handle_t *)t, timer_closed);
}

static void on_timeout(uv_timer_t *t, int stat) {
	lua_State *L = luv_state(t);

	luv_callfield(t, "done_cb", 0, 0);

	if (uv_timer_get_repeat(t) == 0) 
		timer_close(t);
}

static int set_timer(lua_State *L, uv_loop_t *loop, int repeat) {
	int timeout = lua_tonumber(L, 2);

	uv_timer_t *t = (uv_timer_t *)luv_newctx(L, loop, sizeof(uv_timer_t));
	uv_timer_init(loop, t);
	uv_timer_start(t, on_timeout, timeout, repeat ? timeout : 0);

	lua_pushvalue(L, 1);
	lua_setfield(L, -2, "done_cb");

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

	if (t && !uv_is_closing((uv_handle_t *)t)) {
		uv_timer_stop(t);
		timer_close(t);
	}

	return 0;
}

void luv_timer_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "set_timeout", set_timeout);
	luv_register(L, loop, "set_interval", set_interval);
	luv_register(L, loop, "clear_timeout", clear_timer);
	luv_register(L, loop, "clear_interval", clear_timer);
}

