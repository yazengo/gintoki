#pragma once

// format = s16le

void pcm_do_volume(void *_out, int len, float fvol);
void pcm_do_mix(void *_out, void *_in, int len);

void luv_pcm_init(lua_State *L, uv_loop_t *loop);

