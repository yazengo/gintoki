
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>

#include "utils.h"

typedef struct {
	lua_State *L, *Lt;
	uv_loop_t *loop;
	luv_gc_cb gc;
	int refcnt;
} luv_t;

static int __gc(lua_State *L) {
	lua_getfield(L, 1, "_ctx");
	luv_t *l = (luv_t *)lua_touserptr(L, -1);

	if (l->gc)
		l->gc(l->loop, (void *)l + sizeof(luv_t));

	if (l->Lt)
		lua_close(l->Lt);

	free(l);
	return 0;
}

static void lua_newmetatbl(lua_State *L) {
	lua_newtable(L);
	lua_pushliteral(L, "__gc");
	lua_pushcfunction(L, __gc);
	lua_rawset(L, -3);
}

static void lua_pushmaptbl(lua_State *L) {
	lua_pushvalue(L, LUA_REGISTRYINDEX);
}

static void *_new(lua_State *L, uv_loop_t *loop, int size, int usethread) {
	luv_t *l = (luv_t *)zalloc(sizeof(luv_t) + size);
	l->L = L;
	l->loop = loop;

	lua_newtable(L);
	int t = lua_gettop(L);

	lua_newmetatbl(L);
	lua_setmetatable(L, t);

	lua_pushuserptr(L, l);
	lua_setfield(L, t, "_ctx");

	lua_pushmaptbl(L);
	lua_pushlightuserdata(L, l);
	lua_pushvalue(L, t);
	lua_settable(L, -3);
	lua_pop(L, 1);

	if (usethread)
		l->Lt = luaL_newstate();

	l->refcnt++;

	return (void *)l + sizeof(luv_t);
}

static void luv_xmovetable(lua_State *Lsrc, lua_State *Ldst) {
	lua_newtable(Ldst);

	int t = lua_gettop(Lsrc);
	lua_pushnil(Lsrc);
	while (lua_next(Lsrc, t) != 0) {
		// key -2
		// val -1
		lua_pushvalue(Lsrc, -2);
		lua_pushvalue(Lsrc, -2);
		luv_xmove(Lsrc, Ldst, 2);
		lua_settable(Ldst, -3);

		lua_pop(Lsrc, 1);
	}
	lua_pop(Lsrc, 1);
}

static void luv_xmove1(lua_State *Lsrc, lua_State *Ldst) {
	int type = lua_type(Lsrc, -1);

	switch (type) {
	case LUA_TNIL:
		lua_pushnil(Ldst);
		lua_pop(Lsrc, 1);
		break;

	case LUA_TNUMBER:
		lua_pushnumber(Ldst, lua_tonumber(Lsrc, -1));
		lua_pop(Lsrc, 1);
		break;

	case LUA_TBOOLEAN:
		lua_pushboolean(Ldst, lua_toboolean(Lsrc, -1));
		lua_pop(Lsrc, 1);
		break;

	case LUA_TSTRING:
		lua_pushstring(Ldst, lua_tostring(Lsrc, -1));
		lua_pop(Lsrc, 1);
		break;

	case LUA_TTABLE:
		luv_xmovetable(Lsrc, Ldst);
		break;

	default:
		lua_pushnil(Ldst);
		lua_pop(Lsrc, 1);
		break;
	}
}

void luv_xmove(lua_State *Lsrc, lua_State *Ldst, int n) {
	int i = lua_gettop(Ldst);
	while (n--) {
		luv_xmove1(Lsrc, Ldst);
		lua_insert(Ldst, i+1);
	}
}

void *luv_newthreadctx(lua_State *L, uv_loop_t *loop, int size) {
	return _new(L, loop, size, 1);
}

void *luv_newctx(lua_State *L, uv_loop_t *loop, int size) {
	return _new(L, loop, size, 0);
}

void luv_setgc(void *_l, luv_gc_cb cb) {
	luv_t *l = (luv_t *)(_l - sizeof(luv_t));
	l->gc = cb;
}

void luv_pushctx(lua_State *L, void *_l) {
	luv_t *l = (luv_t *)(_l - sizeof(luv_t));

	lua_pushmaptbl(L);
	lua_pushlightuserdata(L, l);
	lua_gettable(L, -2);
	lua_remove(L, -2);
}

void *luv_toctx(lua_State *L, int i) {
	lua_getfield(L, i, "_ctx");
	luv_t *l = (luv_t *)lua_touserptr(L, -1);
	lua_pop(L, 1);

	if (l == NULL)
		return NULL;
	return (void *)l + sizeof(luv_t);
}

void luv_ref(void *_l) {
	luv_t *l = (luv_t *)(_l - sizeof(luv_t));

	l->refcnt++;
}

void luv_unref(void *_l) {
	luv_t *l = (luv_t *)(_l - sizeof(luv_t));
	lua_State *L = l->L;

	l->refcnt--;
	if (l->refcnt > 0)
		return;
	
	lua_pushmaptbl(L);
	lua_pushlightuserdata(L, l);
	lua_pushnil(L);
	lua_settable(L, -3);
	lua_pop(L, 1);

	lua_gc(L, LUA_GCCOLLECT, 0);
}

uv_loop_t *luv_loop(void *_l) {
	luv_t *l = (luv_t *)(_l - sizeof(luv_t));
	return l->loop;
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

static int lua_closure_cb(lua_State *L) {
	luv_t *l = (luv_t *)lua_touserptr(L, lua_upvalueindex(1));
	luv_closure_cb cb = (luv_closure_cb)lua_touserptr(L, lua_upvalueindex(2));
	return cb(L, l->loop, (void *)l + sizeof(luv_t));
}

void luv_pushcclosure(lua_State *L, luv_closure_cb cb, void *_l) {
	lua_pushuserptr(L, _l - sizeof(luv_t));
	lua_pushuserptr(L, cb);
	lua_pushcclosure(L, lua_closure_cb, 2);
}

