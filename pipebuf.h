
#pragma once

typedef struct pipebuf_s {
	void *base;
	int len;
	int refcnt;
	void (*gc)(struct pipebuf_s *pb);
} pipebuf_t;

pbufque_t *pbufque_new();
pbuf_t *pbufque_malloc(pbufque_t *que);

pipebuf_t *pipebuf_new(int len);
void pipebuf_ref(pipebuf_t *pb);
void pipebuf_unref(pipebuf_t *pb);

