
#include <stdlib.h>
#include <string.h>

#include "ringbuf.h"
#include "utils.h"

void ringbuf_init(ringbuf_t *b) {
	b->head = b->tail = b->len = 0;
	b->tailpos = b->headpos = 0;
	memset(&b->getter, 0, sizeof(b->getter));
	b->getter.rb = b;
	memset(&b->putter, 0, sizeof(b->putter));
	b->putter.rb = b;
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

static void filler_init(ringbuf_filler_t *rf, void *buf, int len, ringbuf_done_cb done) {
	if (rf->done)
		panic("last op not complete");
	rf->buf = buf;
	rf->left = rf->len = len;
	rf->done = done;
}

static int filler_get(ringbuf_filler_t *rf) {
	int adv = 0;
	for (;;) {
		void *buf; int len;
		ringbuf_data_ahead_get(rf->rb, &buf, &len);
		if (len == 0 || rf->left == 0)
			break;
		if (len > rf->left)
			len = rf->left;
		memcpy(rf->buf, buf, len);
		rf->left -= len;
		rf->buf += len;
		adv += len;
		ringbuf_push_tail(rf->rb, len);
	}
	if (rf->left == 0 && rf->done) {
		ringbuf_done_cb cb = rf->done;
		rf->done = NULL;
		cb(rf->rb, rf->len - rf->left);
	}
	return adv;
}

static int filler_put(ringbuf_filler_t *rf) {
	int adv = 0;
	for (;;) {
		void *buf; int len;
		ringbuf_space_ahead_get(rf->rb, &buf, &len);
		if (len == 0 || rf->left == 0)
			break;
		if (len > rf->left)
			len = rf->left;
		memcpy(buf, rf->buf, len);
		rf->left -= len;
		rf->buf += len;
		adv += len;
		ringbuf_push_head(rf->rb, len);
	}
	if (rf->left == 0 && rf->done) {
		ringbuf_done_cb cb = rf->done;
		rf->done = NULL;
		cb(rf->rb, rf->len - rf->left);
	}
	return adv;
}

static void filler_fill(ringbuf_t *rb) {
	for (;;) {
		int len = 0;
		len += filler_get(&rb->getter);
		len += filler_put(&rb->putter);
		if (len == 0)
			break;
	}
}

static void filler_cancel(ringbuf_filler_t *rf) {
	if (rf->done) {
		rf->done(rf->rb, rf->len - rf->left);
		rf->done = NULL;
	}
}

void ringbuf_data_get(ringbuf_t *rb, void *buf, int len, ringbuf_done_cb done) {
	filler_init(&rb->getter, buf, len, done);
	filler_fill(rb);
}

void ringbuf_data_put(ringbuf_t *rb, void *buf, int len, ringbuf_done_cb done) {
	filler_init(&rb->putter, buf, len, done);
	filler_fill(rb);
}

void ringbuf_data_cancel_get(ringbuf_t *rb) {
	filler_cancel(&rb->getter);
}

void ringbuf_data_cancel_put(ringbuf_t *rb) {
	filler_cancel(&rb->putter);
}

