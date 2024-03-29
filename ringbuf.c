
#include <stdlib.h>
#include <string.h>

#include "ringbuf.h"
#include "utils.h"

void ringbuf_init(ringbuf_t *b) {
	b->head = b->tail = b->len = 0;
	b->tailpos = b->headpos = 0;
}

void ringbuf_data_ahead_get(ringbuf_t *b, void **_buf, int *_len) {
	int len = RINGBUF_SIZE - b->tail;
	if (b->len < len)
		len = b->len;
	len &= ~3;
	*_len = len;
	*_buf = &b->buf[b->tail];
}

void ringbuf_space_ahead_get(ringbuf_t *b, void **_buf, int *_len) {
	int len = RINGBUF_SIZE - b->head;
	if ((RINGBUF_SIZE - b->len) < len)
		len = RINGBUF_SIZE - b->len;
	len &= ~3;
	*_len = len;
	*_buf = &b->buf[b->head];
}

void ringbuf_push_head(ringbuf_t *b, int len) {
	if (b->len + len > RINGBUF_SIZE)
		panic("len %d too big", len);
	b->head = (b->head + len) % RINGBUF_SIZE;
	b->len += len;
	b->headpos += len;
}

void ringbuf_push_tail(ringbuf_t *b, int len) {
	if (b->len - len < 0)
		panic("len %d too big", len);
	b->tail = (b->tail + len) % RINGBUF_SIZE;
	b->len -= len;
	b->tailpos += len;
}

