
#pragma once

#include "luv.h"
#include "utils.h"
#include "uvwrite.h"
#include "pipe.h"

struct pipe_s;
struct pcopy_s;

typedef void (*pipe_read_cb)(struct pipe_s *p, pipebuf_t *pb);
typedef void (*pipe_write_cb)(struct pipe_s *p, int stat);
typedef void (*pipe_close_cb)(struct pipe_s *p);

typedef struct pipe_s {
	int type;

	uv_stream_t *st;
	uv_pipe_t p;
	uv_file fd;

	void *data;
	unsigned stat;
	pipebuf_t pb;

	struct pcopy_s *copy;

	struct {
		pipe_read_cb done;
	} pread;

	struct {
		pipe_write_cb done;
	} pwrite;

	struct {
		pipebuf_t *pb;
		uv_write_adv_t w;
		pipe_write_cb done;
		immediate_t im_direct, im_stop, im_resume;
		int stat;
	} write;

	struct {
		uv_buf_t pool;
	} direct;

	struct {
		void *buf;
		pipebuf_t *pb;
		pipe_read_cb done;
		immediate_t im_direct, im_stop, im_resume;
	} read;

	struct {
		immediate_t im;
	} close;
} pipe_t;

enum {
	PSTREAM_SRC,
	PSTREAM_SINK,
	PDIRECT_SRC,
	PDIRECT_SINK,
};

enum {
	PS_DOING_R    = (1<<0),
	PS_DOING_W    = (1<<1),
	PS_DONE_R     = (1<<2),
	PS_DONE_W     = (1<<3),
	PS_STOPPED_R  = (1<<4),
	PS_STOPPED_W  = (1<<5),
	PS_PAUSED     = (1<<6),
	PS_STOPPING   = (1<<7),
	PS_CLOSING_R  = (1<<8),
	PS_CLOSING_W  = (1<<9),
	PS_CLOSED     = (1<<10),
	PS_MAX        = (1<<11),
};

void pipe_read(pipe_t *p, pipe_read_cb done);
void pipe_write(pipe_t *p, pipebuf_t *pb, pipe_write_cb done);
void pipe_stop(pipe_t *p);
void pipe_pause(pipe_t *p);
void pipe_resume(pipe_t *p);
void pipe_close_write(pipe_t *p);
void pipe_close_read(pipe_t *p);

