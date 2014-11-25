
#pragma once

#include <uv.h>
#include "queue.h"

typedef struct immediate_s {
	queue_t q;
	void *data;
	void (*cb)(struct immediate_s *);
	uv_async_t *a;
} immediate_t;

void set_immediate(uv_loop_t *loop, immediate_t *im);
void cancel_immediate(immediate_t *im);
void run_immediate();

