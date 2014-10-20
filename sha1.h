
#pragma once

#include <uv.h>
#include <lua.h>

void sha1_digest(void *in, int inlen, void *out);
void luv_sha1_init(lua_State *L, uv_loop_t *loop);

