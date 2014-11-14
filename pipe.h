
#pragma once

#include "luv.h"
#include "utils.h"

struct pipe_s;

typedef uv_buf_t (*pipe_allocbuf_cb)(struct pipe_s *p, int size);
typedef void (*pipe_read_cb)(struct pipe_s *p, uv_buf_t ub);
typedef void (*pipe_write_cb)(struct pipe_s *p, int stat);

typedef struct {
	int type;
	uv_buf_t ub;
} pipeop_t;

enum {
	PO_READ,
	PO_WRITE,
	PO_PAUSE,
	PO_RESUME,
	PO_STOP,
	PO_CLOSE,
};

typedef struct pipe_s {
	int type;

	uv_stream_t *st;
	uv_pipe_t p;
	uv_file fd;

	void *data;
	unsigned flags;

	union {
		struct pipe_s *sink;
		struct pipe_s *src;
	} copy;

	struct {
		uv_buf_t ub;
		uv_write_t w;
		pipe_write_cb done;
		immediate_t im;
		int stat;
	} write;

	struct {
		uv_buf_t ub;
		void *buf;
		int len;
		pipe_allocbuf_cb allocbuf;
		pipe_read_cb done;
		immediate_t im;
	} read;

	struct {
		immediate_t im;
	} forcestop;

	struct {
		immediate_t im;
	} close;
} pipe_t;

enum {
	PT_STREAM,
	PT_DIRECT,
	PT_FILE,
};

void pipe_read(pipe_t *p, pipe_allocbuf_cb allocbuf, pipe_read_cb done);
void pipe_write(pipe_t *p, uv_buf_t ub, pipe_write_cb done);
void pipe_forcestop(pipe_t *p);
void pipe_close(pipe_t *p);

