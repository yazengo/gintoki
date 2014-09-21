
#pragma once

#include <uv.h>
#include <lua.h>

struct audio_in_s;

typedef void (*audio_in_read_cb)(struct audio_in_s *, int len);
typedef void (*audio_in_close_cb)(struct audio_in_s *);

typedef struct audio_in_s {
	void (*on_meta)(struct audio_in_s *ai, const char *key, void *val);

	void (*read)(struct audio_in_s *ai, void *buf, int len, audio_in_read_cb done);
	void (*stop_read)(struct audio_in_s *ai);
	void (*close)(struct audio_in_s *ai, audio_in_close_cb done);

	void *in;
	void *data;
	char *url;
} audio_in_t;

void audio_in_init(uv_loop_t *loop, audio_in_t *ai);
void audio_in_error_init(uv_loop_t *loop, audio_in_t *ai, const char *err);

/*
 * init() -> [ read() -> is_eof? ] -> close()
 *             stop()
 */

