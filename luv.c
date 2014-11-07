
#include <uv.h>
#include <lua.h>
#include <stdlib.h>

#include "utils.h"

/*
 * Usage:
 *
static void on_conn(srv_t *s) {
	lua_State *L = luv_state(s);
	cli_t *c = ..

	luv_new(L, loop, c);
	luv_setfunc(L, -1, "retjson", retjson);
}

static int srv_cancel(lua_State *L, uv_loop_t *loop) {
	srv_t *s = (srv_t *)lua_tohandle(L, 1);

	return 0;
}

static int http_server(lua_State *L, uv_loop_t *loop) {
	
	srv_t *s = ...

	luv_new(L, loop, s);
	luv_setfunc(L, -1, "cancel", srv_cancel);

	luv_free(L, s);

	uv_read(loop);
	
	return 1;
}

lua_State *L = luv_threadstate(s);
lua_pushstring(L, "xx");

luv_newthread(L, loop, s);
lua_xmove(L, luv_threadstate(s), 1);

luv_register(L, loop, "http_server", http_server);
*/

typedef struct {
	lua_State *L, *Lt;
	uv_loop_t *loop;
	char data[0];
} luv_t;

static void *_new(lua_State *L, uv_loop_t *loop, int size, int usethread) {
	luv_t *l = (luv_t *)zalloc(sizeof(luv_t) + size);
	l->L = L;
	l->loop = loop;

	lua_newtable(L);

	lua_pushuserptr(L, l);
	lua_setfield(L, -2, "_luv");

	lua_pushuserptr(L, loop);
	lua_setfield(L, -2, "_loop");

	if (usethread) {
		l->Lt = lua_newthread(L);
		lua_setfield(L, -2, "_thread");
	}

	lua_pushvalue(L, -1);
	lua_setglobalptr(L, "luv", l);

	return l->data;
}

void *luv_newthread(lua_State *L, uv_loop_t *loop, int size) {
	return _new(L, loop, size, 1);
}

void *luv_new(lua_State *L, uv_loop_t *loop, int size) {
	return _new(L, loop, size, 0);
}

static luv_t *_luv_handle(lua_State *L, int i) {
	luv_t *l;
	lua_getfield(L, i, "_luv");
	l = (luv_t *)lua_touserptr(L, -1);
	lua_pop(L, 1);
	return l;
}

void luv_push(lua_State *L, void *_l) {
	luv_t *l = (luv_t *)(_l - sizeof(luv_t));
	lua_getglobalptr(L, "luv", l);
}

void *luv_handle(lua_State *L, int i) {
	return _luv_handle(L, i)->data;
}

void luv_free(void *_l) {
	luv_t *l = (luv_t *)(_l - sizeof(luv_t));
	lua_pushnil(l->L);
	lua_setglobalptr(l->L, "luv", l);
	if (l->Lt)
		lua_close(l->Lt);
	free(l);
}

lua_State *luv_state(void *_l) {
	luv_t *l = (luv_t *)(_l - sizeof(luv_t));
	return l->L;
}

lua_State *luv_threadstate(void *_l) {
	luv_t *l = (luv_t *)(_l - sizeof(luv_t));
	return l->Lt;
}

static int lua_cb(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));
	luv_cb cb = (luv_cb)lua_touserptr(L, lua_upvalueindex(2));
	return cb(L, loop);
}

void luv_register(lua_State *L, uv_loop_t *loop, const char *name, luv_cb cb) {
	lua_pushuserptr(L, loop);
	lua_pushuserptr(L, cb);
	lua_pushcclosure(L, lua_cb, 2);
	lua_setglobal(L, name);
}

void luv_setfunc(lua_State *L, int i, const char *name, luv_cb cb) {
	luv_t *l = _luv_handle(L, i);
	lua_pushuserptr(L, l->loop);
	lua_pushuserptr(L, cb);
	lua_pushcclosure(L, lua_cb, 2);
	lua_setfield(L, i, name);
}

