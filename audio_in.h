
#pragma once

#include <uv.h>
#include <lua.h>

typedef struct audio_in_s {
	void (*on_probe)(struct audio_in_s *ai, const char *key, void *val);
	void (*on_start)(struct audio_in_s *ai, int rate);
	void (*on_exit)(struct audio_in_s *ai);
	void (*on_free)(struct audio_in_s *ai);
	void (*on_read_done)(struct audio_in_s *ai, int len);

	void *in;
	void (*read)(struct audio_in_s *ai, void *buf, int len);
	void (*stop)(struct audio_in_s *ai);

	int is_reading;

	void *data;
	char *url;
} audio_in_t;

void audio_in_read(audio_in_t *ai, void *buf, int len, void (*done)(audio_in_t *ai, int len));
void audio_in_stop(audio_in_t *ai);
int audio_in_is_reading(audio_in_t *ai);

void audio_in_avconv_init(uv_loop_t *loop, audio_in_t *ai);
void audio_in_airplay_init(uv_loop_t *loop, audio_in_t *ai);

void audio_in_airplay_start_loop(lua_State *L, uv_loop_t *loop);
void lua_airplay_init(lua_State *L, uv_loop_t *loop);

