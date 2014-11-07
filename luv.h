
#pragma once

#include <lua.h>
#include <uv.h>

void *luv_newthread(lua_State *L, uv_loop_t *loop, int size);
void *luv_new(lua_State *L, uv_loop_t *loop, int size);
void *luv_handle(lua_State *L, int i);
void luv_push(lua_State *L, void *_l);
void luv_free(void *_l);
lua_State *luv_state(void *_l);
lua_State *luv_threadstate(void *_l);

typedef int (*luv_cb)(lua_State *L, uv_loop_t *loop);
void luv_register(lua_State *L, uv_loop_t *loop, const char *name, luv_cb cb);
void luv_setfunc(lua_State *L, int i, const char *name, luv_cb cb);

