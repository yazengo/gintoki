
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <uv.h>
#include <lua.h>
#include <pthread.h>

#include "utils.h"

static const char *ban[128];
static int ban_nr;
static int ban_all = 0;

void log_ban(const char *file, const char *func) {
	ban[ban_nr*2] = file;
	ban[ban_nr*2+1] = func;
	ban_nr++;
}

static int log_level = LOG_INFO;

void log_set_level(int level) {
	log_level = level;
}

void _log(
	int level,
	const char *func, const char *file, int line,
	char *fmt, ...
) {
	va_list ap;
	char buf[1024];

	if (level < log_level)
		return;

	if (ban_all)
		return;

	int i;
	for (i = 0; i < ban_nr; i++) {
		if (!strcmp(ban[i*2], file)) {
			if (ban[i*2+1] == NULL)
				return;
		 	if (!strcmp(ban[i*2+1], func))
				return;
		}
	}

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	fprintf(stderr, "[%.3f] [%s:%d:%s] %s\n", now(), file, line, func, buf);
}

void log_init() {
	if (getenv("LOG") && !strcmp(getenv("LOG"), "0"))
		ban_all = 1;
}

float now() {
	struct timeval tv;
	static struct timeval tv_start;

	gettimeofday(&tv, NULL);
	if (!tv_start.tv_sec)
		tv_start = tv;
	if (tv.tv_usec - tv_start.tv_usec < 0) {
		tv.tv_usec += 1e6;
		tv.tv_sec--;
	}

	return (tv.tv_sec - tv_start.tv_sec) + (tv.tv_usec - tv_start.tv_usec) / 1e6;
}

typedef struct {
} luv_async_t;

typedef struct {
	lua_State *L;
	luv_cb_t cb;
	uv_async_t *as;
	void *cb_p;
	pthread_mutex_t lock;
} pcall_luv_t;

static void pcall_luv_wrap(uv_async_t *as, int _) {
	pcall_luv_t *p = (pcall_luv_t *)as->data;

	p->cb(p->L, p->cb_p);

	pthread_mutex_unlock(&p->lock);
	uv_close((uv_handle_t *)p->as, NULL);
}

void pthread_call_luv_sync(lua_State *L, uv_loop_t *loop, luv_cb_t cb, void *cb_p) {
	pcall_luv_t p = {
		.L = L, .cb = cb, .cb_p = cb_p,
		.lock = PTHREAD_MUTEX_INITIALIZER,
	};
	pthread_mutex_lock(&p.lock);
	uv_async_t as;
	as.data = &p;
	p.as = &as;
	uv_async_init(loop, &as, pcall_luv_wrap);
	uv_async_send(&as);

	pthread_mutex_lock(&p.lock);
	pthread_mutex_destroy(&p.lock);
}

