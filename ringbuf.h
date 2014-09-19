#pragma once

#include <uv.h>

#define RINGBUF_SIZE (1024*24)

struct ringbuf_s;

typedef void (*ringbuf_done_cb)(struct ringbuf_s *rb, int len);

typedef struct {
	void *buf;
	int left, len;
	struct ringbuf_s *rb;
	ringbuf_done_cb done;
} ringbuf_filler_t;

//    >-------- tailpos ----------- headpos -------> 
//   head == headpos % RINGBUF_SIZE
//   tail == tailpos % RINGBUF_SIZE
typedef struct ringbuf_s {
	char buf[RINGBUF_SIZE];
	int head, tail, len;
	int headpos, tailpos;
	void *data;
	ringbuf_filler_t getter, putter;
	uv_loop_t *loop;
} ringbuf_t;

void ringbuf_init(ringbuf_t *b, uv_loop_t *loop);
void ringbuf_data_ahead_get(ringbuf_t *b, void **_buf, int *_len);
void ringbuf_space_ahead_get(ringbuf_t *b, void **_buf, int *_len);
void ringbuf_push_head(ringbuf_t *b, int len);
void ringbuf_push_tail(ringbuf_t *b, int len);

void ringbuf_data_get(ringbuf_t *b, void *buf, int len, ringbuf_done_cb done);
void ringbuf_data_put(ringbuf_t *b, void *buf, int len, ringbuf_done_cb done);

void ringbuf_data_put_force(ringbuf_t *b, void *buf, int len);

void ringbuf_data_cancel_get(ringbuf_t *rb);
void ringbuf_data_cancel_put(ringbuf_t *rb);

