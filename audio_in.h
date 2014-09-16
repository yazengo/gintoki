
#pragma once

#include <uv.h>
#include <lua.h>

struct audio_in_s;

typedef void (*audio_in_read_cb)(struct audio_in_s *, int len);
typedef void (*audio_in_close_cb)(struct audio_in_s *);

typedef struct audio_in_s {
	void (*on_probe)(struct audio_in_s *ai, const char *key, void *val);
	void (*on_start)(struct audio_in_s *ai, int rate);

	int (*can_read)(struct audio_in_s *ai);
	int (*is_eof)(struct audio_in_s *ai);

	void (*read)(struct audio_in_s *ai, void *buf, int len, audio_in_read_cb done);
	void (*stop)(struct audio_in_s *ai);
	void (*close)(struct audio_in_s *ai, audio_in_close_cb done);

	void *in;
	void *data;
	char *url;
} audio_in_t;

void audio_in_init(uv_loop_t *loop, audio_in_t *ai);

/*
 * init() -> [ can_read() -> read() ] -> is_eof() -> close()
 *                      stop()
 */

