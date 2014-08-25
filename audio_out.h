#pragma once

#include <ao/ao.h>
#include <uv.h>

typedef struct audio_out_s {
	uv_loop_t *loop;

	int is_playing;
	void *data;

	void *play_buf;
	int play_len;

	int rate;

	void (*on_play_done)(struct audio_out_s *ao, int len);

	ao_device *aodev;
	int fd_oss;

	void (*init)(struct audio_out_s *ao);
	void (*set_rate)(struct audio_out_s *ao, int rate);
	void (*play)(struct audio_out_s *ao, void *buf, int len);

} audio_out_t;

void audio_out_init(uv_loop_t *loop, audio_out_t *ao, int sample_rate);
void audio_out_set_rate(audio_out_t *ao, int rate);
void audio_out_play(audio_out_t *ao, void *buf, int len, void (*done)(audio_out_t *, int));
void audio_out_cancel_play(audio_out_t *ao);
int audio_out_is_playing(audio_out_t *ao);

