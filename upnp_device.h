#pragma once

#include <uv.h>
#include <lua.h>

void upnp_init(lua_State *L, uv_loop_t *loop);
void upnp_start();

