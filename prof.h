
#pragma once

typedef struct {
	const char *name;
	int nr;
	int rx, tx;
} prof_t;

static inline void prof_inc(prof_t *p) {
	p->nr++;
}

