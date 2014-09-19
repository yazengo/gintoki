
#pragma once

#include <uv.h>

#include "ringbuf.h"
#include "audio_in.h"

struct audiosrc_srv_s;
struct audiosrc_cli_s;

typedef void (*audiosrc_srv_done_cb)(struct audiosrc_srv_s *as);
typedef void (*audiosrc_cli_done_cb)(struct audiosrc_cli_s *ac);

typedef struct audiosrc_srv_s {
	int stat;

	char *url;
	int i;
	void *data;

	struct audiosrc_cli_s *ac;
} audiosrc_srv_t;

typedef struct audiosrc_cli_s {
	int stat;

	ringbuf_t buf;
	audio_in_t *ai;
	uv_loop_t *loop;

	audiosrc_cli_done_cb on_write_done;
	audio_in_read_cb on_read_done;
} audiosrc_cli_t;

void audiosrc_srv_start(uv_loop_t *loop, audiosrc_srv_t *as);
void audiosrc_srv_write(audiosrc_srv_t *as, void *buf, int len);
void audiosrc_srv_stop(audiosrc_srv_t *as);

void audio_in_audiosrc_init(uv_loop_t *loop, audio_in_t *ai);

