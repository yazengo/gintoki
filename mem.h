
#pragma once

#include "queue.h"

typedef struct {
	const char *name;
	int size;
	queue_t objs;
	int inuse;
	int freed;
} objpool_t;

typedef struct {
	queue_t q;
} obj_t;

void *objpool_get(objpool_t *p);
void objpool_put(objpool_t *p, void *obj);

