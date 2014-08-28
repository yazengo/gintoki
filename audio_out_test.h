#pragma once

#include <uv.h>

void test_audio_out(uv_loop_t *loop);
void audio_out_test_fill_buf_with_key(void *buf, int len, int rate, int key);

