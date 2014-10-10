
#pragma once

#include <uv.h>
#include <lua.h>

#include "audio_in.h"

void luv_airplay_init_v2(lua_State *L, uv_loop_t *loop);
void audio_in_airplay_init_v2(uv_loop_t *loop, audio_in_t *ai);

