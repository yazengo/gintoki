
#pragma once

#include "mem.h"
#include "queue.h"

typedef struct pipebuf_s {
	void *base;
	int len;
	int refcnt;
	void (*gc)(struct pipebuf_s *pb);
	char buf[0];
	queue_t q;
} pipebuf_t;

extern int PIPEBUF_SIZE;

pipebuf_t *pipebuf_new();
void pipebuf_ref(pipebuf_t *pb);
void pipebuf_unref(pipebuf_t *pb);

