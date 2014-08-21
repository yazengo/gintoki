#pragma once

#include <uv.h>

void audio_out_init(uv_loop_t *loop, int sample_rate);
void audio_out_play(void *buf, int len, void (*cb)(void *), void *cb_p);

