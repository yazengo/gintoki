
#pragma once

#define PIPEBUF_SIZE 2048

typedef struct pipebuf_s {
	void *base;
	int len;
	int refcnt;
	void (*gc)(struct pipebuf_s *pb);
} pipebuf_t;

pipebuf_t *pipebuf_new();
void pipebuf_ref(pipebuf_t *pb);
void pipebuf_unref(pipebuf_t *pb);

