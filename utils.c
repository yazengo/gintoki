
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
	lua_State *L;
	luv_cb_t cb;
	void *cb_p;
	pthread_mutex_t lock;
} pcall_luv_t;

static void pcall_luv_wrap(uv_async_t *as, int _) {
	pcall_luv_t *p = (pcall_luv_t *)as->data;

	p->cb(p->L, p->cb_p);

	pthread_mutex_unlock(&p->lock);
}

void pthread_call_luv_sync(lua_State *L, uv_loop_t *loop, luv_cb_t cb, void *cb_p) {
	pcall_luv_t p = {
		.L = L, .cb = cb, .cb_p = cb_p,
		.lock = PTHREAD_MUTEX_INITIALIZER,
	};
	pthread_mutex_lock(&p.lock);

	uv_async_t as;
	uv_async_init(loop, &as, pcall_luv_wrap);
	as.data = &p;
	uv_async_send(&as);

	pthread_mutex_lock(&p.lock);
	pthread_mutex_destroy(&p.lock);
}

typedef struct {
	lua_State *L;
	luv_cb_t on_start;
	luv_cb_t on_done;
	void *cb_p;
	pthread_mutex_t lock;
} pcall_luv_v2_t;

static int pcall_luv_v2_done(lua_State *L) {
	void *ud = lua_touserdata(L, lua_upvalueindex(1));
	pcall_luv_v2_t *p;
	memcpy(&p, ud, sizeof(p));

	if (p == NULL)
		return 0;
	memset(ud, 0, sizeof(p));

	p->on_done(p->L, p->cb_p);
	pthread_mutex_unlock(&p->lock);

	return 0;
}

static void pcall_luv_v2_wrap(uv_async_t *as, int _) {
	pcall_luv_v2_t *p = (pcall_luv_v2_t *)as->data;

	void *ud = lua_newuserdata(p->L, sizeof(p));
	memcpy(ud, &p, sizeof(p));
	lua_pushcclosure(p->L, pcall_luv_v2_done, 1);

	p->on_start(p->L, p->cb_p);

	lua_pop(p->L, 1);
}

void pthread_call_luv_sync_v2(lua_State *L, uv_loop_t *loop, luv_cb_t on_start, luv_cb_t on_done, void *cb_p) {
	pcall_luv_v2_t p = {
		.L = L, .cb_p = cb_p,
		.on_start = on_start,
		.on_done = on_done,
		.lock = PTHREAD_MUTEX_INITIALIZER,
	};
	pthread_mutex_lock(&p.lock);

	uv_async_t as;
	uv_async_init(loop, &as, pcall_luv_v2_wrap);
	as.data = &p;
	uv_async_send(&as);

	pthread_mutex_lock(&p.lock);
	pthread_mutex_destroy(&p.lock);
}

static int timer_cb_inner(lua_State *L) {
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_call(L, 0, 0);

	return 0;
}

static void timer_cb(uv_timer_t *t, int _) {
	lua_State *L = (lua_State *)t->data;

	uv_timer_stop(t);
	free(t);

	char name[64];
	sprintf(name, "timer_%p", t);

	lua_getglobal(L, name);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return;
	}
	lua_call(L, 0, 0);

	lua_pushnil(L);
	lua_setglobal(L, name);
}

static int set_timeout(lua_State *L) {
	uv_loop_t *loop;
	void *ud = lua_touserdata(L, lua_upvalueindex(1));
	memcpy(&loop, ud, sizeof(loop));

	//   -1  timeout
	//   -2  callback
	int timeout = lua_tonumber(L, -1);
	lua_pop(L, 1);

	//   -1  callback
	lua_pushcclosure(L, timer_cb_inner, 1);

	uv_timer_t *t = (uv_timer_t *)malloc(sizeof(uv_timer_t));
	uv_timer_init(loop, t);
	t->data = L;

	char name[64];
	sprintf(name, "timer_%p", t);
	lua_setglobal(L, name);

	//info("%s timeout=%d", name, timeout);
	uv_timer_start(t, timer_cb, timeout, timeout);

	// return timer_xxx
	lua_pushstring(L, name);
	return 1;
}

static int info_lua(lua_State *L) {
	const char *msg = lua_tostring(L, -1);
	info("%s", msg);
	return 0;
}

void utils_init(lua_State *L, uv_loop_t *loop) {
	void *ud = lua_newuserdata(L, sizeof(loop));
	memcpy(ud, &loop, sizeof(loop));
	lua_pushcclosure(L, set_timeout, 1);
	lua_setglobal(L, "set_timeout");

	lua_pushcfunction(L, info_lua);
	lua_setglobal(L, "info");
}

