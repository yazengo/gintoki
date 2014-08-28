#pragma once

#define RINGBUF_SIZE (1024*24)

struct ringbuf_s;

typedef void (*ringbuf_done_cb)(struct ringbuf_s *rb, int len);

//    >-------- tailpos ----------- headpos -------> 
//   head == headpos % RINGBUF_SIZE
//   tail == tailpos % RINGBUF_SIZE
typedef struct ringbuf_s {
	char buf[RINGBUF_SIZE];
	int head, tail, len;
	int headpos, tailpos;

	void *getbuf;
	int getlen, getlen_orig;
	ringbuf_done_cb on_get_done;

	void *putbuf;
	int putlen, putlen_orig;
	ringbuf_done_cb on_put_done;

} ringbuf_t;

void ringbuf_init(ringbuf_t *b);
void ringbuf_data_ahead_get(ringbuf_t *b, void **_buf, int *_len);
void ringbuf_space_ahead_get(ringbuf_t *b, void **_buf, int *_len);
void ringbuf_push_head(ringbuf_t *b, int len);
void ringbuf_push_tail(ringbuf_t *b, int len);

void ringbuf_data_get(ringbuf_t *b, void *buf, int len, ringbuf_done_cb done);
void ringbuf_data_put(ringbuf_t *b, void *buf, int len, ringbuf_done_cb done);

void ringbuf_data_cancel_get(ringbuf_t *rb);
void ringbuf_data_cancel_put(ringbuf_t *rb);

