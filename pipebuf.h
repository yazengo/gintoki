
#pragma once

#include "mem.h"

typedef struct pipebuf_s {
	void *base;
	int len;
	int refcnt;
	void (*gc)(struct pipebuf_s *pb);
	char buf[0];
} pipebuf_t;

#define PIPEBUF_ALLOCSIZE (4096 - sizeof(obj_t))
#define PIPEBUF_SIZE (PIPEBUF_ALLOCSIZE - sizeof(pipebuf_t))

pipebuf_t *pipebuf_new();
void pipebuf_ref(pipebuf_t *pb);
void pipebuf_unref(pipebuf_t *pb);

