
#pragma once

#include <lua.h>
#include <uv.h>

typedef int (*luv_cb)(lua_State *L, uv_loop_t *loop);
typedef void (*luv_gc_cb)(uv_loop_t *loop, void *_l);

void *luv_newthreadctx(lua_State *L, uv_loop_t *loop, int size);
void *luv_newctx(lua_State *L, uv_loop_t *loop, int size);

void luv_setgc(void *_l, luv_gc_cb cb);

void luv_setfield(lua_State *L, int t, const char *k);
void luv_getfield(lua_State *L, int t, const char *k);

void *luv_toctx(lua_State *L, int i);
void luv_pushctx(lua_State *L, void *_l);

lua_State *luv_state(void *_l);
lua_State *luv_threadstate(void *_l);

void luv_unref(void *_l);

void luv_register(lua_State *L, uv_loop_t *loop, const char *name, luv_cb cb);
void luv_setfunc(lua_State *L, int i, const char *name, luv_cb cb);

