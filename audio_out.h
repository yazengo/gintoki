#pragma once

#include <ao/ao.h>
#include <uv.h>

typedef struct audio_out_s {
	uv_loop_t *loop;

	void *data;

	void *play_buf;
	int play_len;

	void (*on_play_done)(struct audio_out_s *ao, int len);

	ao_device *aodev;
} audio_out_t;

void audio_out_init(uv_loop_t *loop, audio_out_t *ao, int sample_rate);
void audio_out_set_rate(audio_out_t *ao, int rate);
void audio_out_play(audio_out_t *ao, void *buf, int len, void (*done)(audio_out_t *, int));
void audio_out_cancel_play(audio_out_t *ao);

