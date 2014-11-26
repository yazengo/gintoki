
#pragma once

#include "luv.h"
#include "utils.h"
#include "uvwrite.h"
#include "queue.h"

struct pipe_s;

typedef void (*pipe_read_cb)(struct pipe_s *p, pipebuf_t *pb);
typedef void (*pipe_write_cb)(struct pipe_s *p, int stat);
typedef void (*pipe_close_cb)(struct pipe_s *p);

typedef struct pipe_s {
	int type;

	uv_stream_t *st;
	uv_pipe_t p;
	uv_file fd;

	queue_t q;

	int stat;
	int rdstat;
	int wrstat;

	struct {
		pipebuf_t *pb;
		uv_write_adv_t w;
		pipe_write_cb done;
		immediate_t im_direct, im_stop, im_resume;
		int stat;
		void *data;
	} write;

	struct {
		pipebuf_t *pool;
	} direct;

	struct {
		void *buf;
		pipebuf_t *pb;
		int len;
		pipe_read_cb done;
		immediate_t im_direct, im_stop, im_resume;
		void *data;
		int mode;
	} read;

	struct {
		immediate_t im;
	} close_read;

	struct {
		immediate_t im;
	} close_write;
} pipe_t;

enum {
	PREAD_NORMAL,
	PREAD_BLOCK,
};

enum {
	PSTREAM_SRC,
	PSTREAM_SINK,
	PDIRECT_SRC,
	PDIRECT_SINK,
};

void pipe_read(pipe_t *p, pipe_read_cb done);
void pipe_write(pipe_t *p, pipebuf_t *pb, pipe_write_cb done);
void pipe_close_read(pipe_t *p);
void pipe_close_write(pipe_t *p);
void pipe_cancel_read(pipe_t *p);
void pipe_cancel_write(pipe_t *p);

