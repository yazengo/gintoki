#pragma once

#define RINGBUF_SIZE (1024*24)

//    >-------- tailpos ----------- headpos -------> 
//   head == headpos % RINGBUF_SIZE
//   tail == tailpos % RINGBUF_SIZE
typedef struct {
	char buf[RINGBUF_SIZE];
	int head, tail, len;
	int headpos, tailpos;
} ringbuf_t;

void ringbuf_init(ringbuf_t *b);
void ringbuf_data_ahead_get(ringbuf_t *b, void **_buf, int *_len);
void ringbuf_space_ahead_get(ringbuf_t *b, void **_buf, int *_len);
void ringbuf_push_head(ringbuf_t *b, int len);
void ringbuf_push_tail(ringbuf_t *b, int len);

