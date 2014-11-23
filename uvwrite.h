
#pragma once

#include <uv.h>
#include "pipebuf.h"

struct uv_write_adv_s;

typedef void (*uv_write_adv_cb)(struct uv_write_adv_s *wa, int stat);

typedef struct uv_write_adv_s {
	int stat;
	void *data;
	pipebuf_t *pb, *pb_pending;
	uv_buf_t ub;
	uv_write_t w;
	uv_write_adv_cb done;
	uv_stream_t *st;
} uv_write_adv_t;

void uv_write_adv(uv_write_adv_t *wa, pipebuf_t *pb, uv_write_adv_cb done);
void uv_write_adv_cancel(uv_write_adv_t *wa);
void uv_write_adv_close(uv_write_adv_t *wa, uv_close_cb done);

