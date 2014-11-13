
#pragma once

#include "luv.h"

struct pcopy_s;

typedef struct pipe_s {
	int type;
	uv_stream_t *st;
	uv_pipe_t p;
	uv_file fd;
	void *data;
	struct pcopy_s *cpy;
} pipe_t;

enum {
	PT_STREAM,
	PT_DIRECT,
	PT_FILE,
};

typedef uv_buf_t (*pipe_allocbuf_cb)(pipe_t *p, int size);
typedef uv_buf_t (*pipe_read_cb)(pipe_t *p, uv_buf_t ub);
typedef uv_buf_t (*pipe_write_cb)(pipe_t *p, int stat);

void pipe_read(pipe_t *p, pipe_allocbuf_cb allocbuf, pipe_read_cb done);
void pipe_close(pipe_t *p);

