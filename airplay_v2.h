
#pragma once

#include <uv.h>
#include <lua.h>

#include "audio_in.h"

void luv_airplay_v2_init(lua_State *L, uv_loop_t *loop);
void audio_in_airplay_v2_init(uv_loop_t *loop, audio_in_t *ai);

