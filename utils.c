
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <stdarg.h>
#include <execinfo.h>
#include <signal.h>
#include <dirent.h>

#include <uv.h>
#include <lua.h>
#include <lauxlib.h>
#include <pthread.h>

#include "utils.h"

static int log_level = LOG_INFO;
static uv_loop_t *g_loop;

static void print_traceback_and_exit();

void setloglevel(int level) {
	log_level = level;
}

void _log(
	int level,
	const char *func, const char *file, int line,
	char *fmt, ...
) {
	va_list ap;
	char buf[512];

	if (level < log_level)
		return;

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf)-2, fmt, ap);
	va_end(ap);

	fprintf(stderr, "[%8.3f] [%s:%d:%s] %s\n", now(), file, line, func, buf);

	if (level == LOG_PANIC)
		print_traceback_and_exit();
}

float now() {
	static uint64_t tm;
	if (tm == 0)
		tm = uv_now(g_loop);
	return (float)(uv_now(g_loop) - tm)/1e3;
}

void *zalloc(int len) {
	void *p = malloc(len);
	if (p == NULL)
		panic("no memory");
	memset(p, 0, len);
	return p;
}

void *memdup(void *buf, int len) {
	void *p = malloc(len);
	if (p == NULL)
		panic("no memory");
	memcpy(p, buf, len);
	return p;
}

void print_traceback() {
	fprintf(stderr, "native traceback:\n");
	void *array[128];
	size_t size;
	size = backtrace(array, 128);
	backtrace_symbols_fd(array, size, 2);
	fprintf(stderr, "\n");
}

void _lua_dumpstack_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L) {
	int i;
	_log(LOG_INFO, at_func, at_file, at_lineno, "==== top=%d", lua_gettop(L));
	for (i = -lua_gettop(L); i <= -1; i++) {
		int type = lua_type(L, i);
		const char *typename = lua_typename(L, type);
		char str[128] = {};

		switch (type) {
		case LUA_TNUMBER:
			sprintf(str, ": %lf", lua_tonumber(L, i));
			break;
		case LUA_TBOOLEAN:
			sprintf(str, ": %s", lua_toboolean(L, i) ? "true": "false");
			break;
		case LUA_TSTRING:
			sprintf(str, ": '%s'", lua_tostring(L, i));
			break;
		default:
			sprintf(str, ": %p", lua_topointer(L, i));
			break;
		}
		_log(LOG_INFO, at_func, at_file, at_lineno, 
			"%d %s%s", i, typename, str
		);
	}
}

static int lua_log(lua_State *L) {
	int level = lua_tonumber(L, 1);
	const char *func = lua_tostring(L, 2);
	const char *file = lua_tostring(L, 3);
	int line = lua_tonumber(L, 4);
	const char *msg = lua_tostring(L, 5);

	_log(level, func, file, line, "%s", msg);
	return 0;
}

static int lua_setloglevel(lua_State *L) {
	setloglevel(lua_tonumber(L, -1));
	return 0;
}

static int lua_traceback(lua_State *L) {
  const char *msg = lua_tostring(L, 1);
  if (msg)
    luaL_traceback(L, L, msg, 1);
  else if (!lua_isnoneornil(L, 1)) {  /* is there an error object? */
    if (!luaL_callmeta(L, 1, "__tostring"))  /* try its 'tostring' metamethod */
      lua_pushliteral(L, "(no error message)");
  }
  return 1;
}

static int lua_docall(lua_State *L, int narg, int nres) {
  int status;
  int base = lua_gettop(L) - narg;  /* function index */
  lua_pushcfunction(L, lua_traceback);  /* push traceback function */
  lua_insert(L, base);  /* put it under chunk and args */
  status = lua_pcall(L, narg, nres, base);
  lua_remove(L, base);  /* remove traceback function */
  return status;
}

void lua_dostring_or_die_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L, const char *str) {
	if (luaL_dostring(L, str)) {
		_log(LOG_PANIC, at_func, at_file, at_lineno, "lua_dostring: %s", lua_tostring(L, -1));
	}
}

void lua_dofile_or_die_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L, char *fname) {
	int r = luaL_loadfile(L, fname);
	if (r) {
		if (r == LUA_ERRFILE) 
			_log(LOG_PANIC, at_func, at_file, at_lineno, "'%s' open failed", fname);
		else if (r == LUA_ERRSYNTAX) 
			_log(LOG_PANIC, at_func, at_file, at_lineno, "'%s' has syntax error", fname);
		else 
			_log(LOG_PANIC, at_func, at_file, at_lineno, "'%s' has unknown error", fname);
	}
	lua_call_or_die_at(at_func, at_file, at_lineno, L, 0, LUA_MULTRET);
}

void lua_call_or_die_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L, int nargs, int nresults) {
	if (lua_docall(L, nargs, nresults)) {
		_log(LOG_PANIC, at_func, at_file, at_lineno, "lua_call: %s", lua_tostring(L, -1));
	}
}

void lua_pushuserptr(lua_State *L, void *p) {
	void *ud = lua_newuserdata(L, sizeof(p));
	memcpy(ud, &p, sizeof(p));
}

void lua_pushuserdata(lua_State *L, void *p, int len) {
	void *ud = lua_newuserdata(L, sizeof(p));
	memcpy(ud, p, len);
}

void *lua_touserptr(lua_State *L, int index) {
	void *p;
	void *ud = lua_touserdata(L, index);
	memcpy(&p, ud, sizeof(p));
	return p;
}

typedef void (*onexit_cb)();

#define EXITCB_NR 16
static onexit_cb exitcbs[EXITCB_NR];

static void print_traceback_and_exit() {
	print_traceback();

	int i;
	for (i = 0; i < EXITCB_NR; i++)
		if (exitcbs[i])
			exitcbs[i]();

	exit(-1);
}

static void term(int sig) {
	signal(SIGTERM, SIG_IGN);
	error("sig=%d", sig);
	print_traceback_and_exit();
}

static void fault(int sig) {
	signal(SIGILL, SIG_IGN);
	signal(SIGBUS, SIG_IGN);
	signal(SIGSEGV, SIG_IGN);
	signal(SIGABRT, SIG_IGN);
	error("sig=%d", sig);
	print_traceback_and_exit();
}

void utils_preinit(uv_loop_t *loop) {
	g_loop = loop;

	if (getenv("COREDUMP") == NULL) {
		signal(SIGILL, fault);
		signal(SIGBUS, fault);
		signal(SIGSEGV, fault);
		signal(SIGABRT, fault);
	} else 
		info("coredump enabled");

	signal(SIGTERM, term);

	char *s = getenv("LOG");
	if (s) {
		int v = 1;
		sscanf(s, "%d", &v);
		setloglevel(v);
	}
}

static void immediate_closed(uv_handle_t *h) {
	free(h);
}

static void immediate_cb(uv_check_t *t, int stat) {
	immediate_t *im = (immediate_t *)t->data;
	im->t = NULL;
	uv_close((uv_handle_t *)t, immediate_closed);
	im->cb(im);
}

void cancel_immediate(immediate_t *im) {
	if (im->t) {
		uv_check_stop(im->t);
		uv_close((uv_handle_t *)im->t, immediate_closed);
		im->t = NULL;
	}
}

void set_immediate(uv_loop_t *loop, immediate_t *im) {
	uv_check_t *t = (uv_check_t *)zalloc(sizeof(uv_check_t));
	im->t = t;
	t->data = im;
	uv_check_init(loop, t);
	uv_check_start(t, immediate_cb);
}

void luv_utils_init(lua_State *L, uv_loop_t *loop) {
	lua_pushcfunction(L, lua_log);
	lua_setglobal(L, "_log");

	lua_pushcfunction(L, lua_setloglevel);
	lua_setglobal(L, "setloglevel");
}

