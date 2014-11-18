
#pragma once

#include "luv.h"

struct pipe_s;

typedef struct {
	struct pipe_s *src, *sink;
	char rbuf[1024];
	char wbuf[1024];
	uv_buf_t wub;
	uv_write_t wr;
} pipecopy_t;

typedef struct pipe_s {
	int type;
	uv_stream_t *st;
	uv_pipe_t p;
	void *data;
	pipecopy_t *cpy;
} pipe_t;

