#pragma once

#include <sys/soundcard.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavresample/avresample.h>
#include <libavutil/mathematics.h>

#include <uv.h>

typedef struct audio_out_s {
	AVOutputFormat *ofmt;
	AVFormatContext *fmt_ctx;
	AVCodec *codec;
	AVStream *st;

	int fd_oss;

	uv_loop_t *loop;

	void *data;

	void *play_buf;
	int play_len;

	void (*on_play_done)(struct audio_out_s *);
} audio_out_t;

void audio_out_init(uv_loop_t *loop, audio_out_t *ao, int sample_rate);
void audio_out_play(audio_out_t *ao, void *buf, int len, void (*done)(audio_out_t *));

