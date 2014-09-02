
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static const char *ban[128];
static int ban_nr;
static int ban_all = 0;

void log_ban(const char *file, const char *func) {
	ban[ban_nr*2] = file;
	ban[ban_nr*2+1] = func;
	ban_nr++;
}

static int log_level = LOG_INFO;

void setloglevel(int level) {
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

	fprintf(stdout, "[%.3f] [%s:%d:%s] %s\n", now(), file, line, func, buf);

	if (level == LOG_PANIC) {
		print_trackback();
		exit(-1);
	}
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

void *zalloc(int len) {
	void *p = malloc(len);
	if (p == NULL)
		panic("no memory");
	memset(p, 0, len);
	return p;
}

void print_trackback() {
	fprintf(stderr, "native trackback:\n");
	void *array[128];
	size_t size;
	size = backtrace(array, 128);
	backtrace_symbols_fd(array, size, 2);
}

typedef struct {
	lua_State *L;

	lua_CFunction on_start;
	lua_CFunction on_done;

	void *data;
	pthread_mutex_t lock;
} pcall_luv_v2_t;

static void pcall_luv_v2_handle_free(uv_handle_t *h) {
	free(h);
}

static int pcall_luv_v2_done(lua_State *L) {
	pcall_luv_v2_t *p = (pcall_luv_v2_t *)lua_touserptr(L, lua_upvalueindex(1));
	if (p == NULL)
		return 0;
	lua_setuserptr(L, lua_upvalueindex(1), NULL);

	lua_pushcfunction(p->L, p->on_done);

	// arg[1] = data
	lua_pushuserptr(p->L, p->data);

	// arg[2] = ret
	lua_pushvalue(p->L, 1);

	lua_call_or_die(p->L, 2, 0);

	pthread_mutex_unlock(&p->lock);

	return 0;
}

static void pcall_luv_v2_start(uv_async_t *as, int _) {
	pcall_luv_v2_t *p = (pcall_luv_v2_t *)as->data;

	lua_pushcfunction(p->L, p->on_start);

	// arg[1] = data
	lua_pushuserptr(p->L, p->data);

	// arg[2] = done
	lua_pushuserptr(p->L, p);
	lua_pushcclosure(p->L, pcall_luv_v2_done, 1);

	lua_call_or_die(p->L, 2, 0);

	uv_close((uv_handle_t *)as, pcall_luv_v2_handle_free);
}

void pthread_call_luv_sync_v2(lua_State *L, uv_loop_t *loop, lua_CFunction on_start, lua_CFunction on_done, void *data) {
	pcall_luv_v2_t p = {
		.L = L, .data = data,
		.on_start = on_start,
		.on_done = on_done,
		.lock = PTHREAD_MUTEX_INITIALIZER,
	};
	pthread_mutex_lock(&p.lock);

	uv_async_t *as = (uv_async_t *)zalloc(sizeof(uv_async_t));
	uv_async_init(loop, as, pcall_luv_v2_start);
	as->data = &p;
	uv_async_send(as);

	pthread_mutex_lock(&p.lock);
	pthread_mutex_destroy(&p.lock);
}

typedef struct {
	pcall_uv_cb cb;
	void *cb_p;
	pthread_mutex_t lock;
	const char *name;
} pcall_uv_t;

void pthread_call_uv_complete(void *_p) {
	pcall_uv_t *p = (pcall_uv_t *)_p;
	pthread_mutex_unlock(&p->lock);
}

static void pcall_uv_handle_free(uv_handle_t *h) {
	free(h);
}

static void pcall_uv_done(uv_async_t *as, int _) {
	pcall_uv_t *p = (pcall_uv_t *)as->data;
	debug("async_call %s:%p", p->name, as);
	p->cb(p, p->cb_p);
	uv_close((uv_handle_t *)as, pcall_uv_handle_free);
}

void pthread_call_uv_wait_withname(uv_loop_t *loop, pcall_uv_cb cb, void *cb_p, const char *name) {
	pcall_uv_t p = {
		.lock = PTHREAD_MUTEX_INITIALIZER,
		.cb = cb, .cb_p = cb_p,
		.name = name,
	};
	pthread_mutex_lock(&p.lock);

	uv_async_t *as = (uv_async_t *)zalloc(sizeof(uv_async_t));
	uv_async_init(loop, as, pcall_uv_done);
	as->data = &p;
	debug("async_send %s:%p", name, as);
	uv_async_send(as);

	pthread_mutex_lock(&p.lock);
	pthread_mutex_destroy(&p.lock);
}

void pthread_call_uv_wait(uv_loop_t *loop, pcall_uv_cb cb, void *cb_p) {
	pthread_call_uv_wait_withname(loop, cb, cb_p, "normal");
}

typedef struct {
	void (*cb)(void *);
	void *p;
} call_soon_t;

static void call_soon_handle_free(uv_handle_t *h) {
	free(h);
}

static void call_soon_done(uv_async_t *as, int _) {
	call_soon_t *c = (call_soon_t *)as->data;
	c->cb(c->p);
	free(c);
	uv_close((uv_handle_t *)as, call_soon_handle_free);
}

void uv_call_soon(uv_loop_t *loop, void (*done)(void *), void *p) {
	call_soon_t *c = (call_soon_t *)zalloc(sizeof(call_soon_t));

	uv_async_t *as = (uv_async_t *)zalloc(sizeof(uv_async_t));
	uv_async_init(loop, as, call_soon_done);
	as->data = c;
	uv_async_send(as);
}

static void timer_free(uv_handle_t *t) {
	free(t);
}

static void timeout_alarm(uv_timer_t *t, int _) {
	uv_timeout_t *to = (uv_timeout_t *)t->data;
	uv_timer_stop(t);
	to->timeout_cb(to);
	uv_close((uv_handle_t *)t, timer_free);
}

void uv_set_timeout(uv_loop_t *loop, uv_timeout_t *to) {
	uv_timer_t *t = (uv_timer_t *)zalloc(sizeof(uv_timer_t));
	t->data = to;
	uv_timer_init(loop, t);
	uv_timer_start(t, timeout_alarm, to->timeout, to->timeout);
}

static int luv_timeout_cb_inner(lua_State *L) {
	//info("is_function %d", lua_isfunction(L, lua_upvalueindex(1)));
	lua_pushvalue(L, lua_upvalueindex(1));
	lua_call_or_die(L, 0, 0);
	return 0;
}

static void luv_timeout_cb(uv_timeout_t *to) {
	lua_State *L = (lua_State *)to->data;
	lua_do_global_callback(L, "timer", to, 0, 1);
	free(to);
}

// arg[1] = callback
// arg[2] = timeout
static int luv_set_timeout(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));
	uv_timeout_t *to = (uv_timeout_t *)zalloc(sizeof(uv_timeout_t));

	to->timeout = lua_tonumber(L, 2);
	to->timeout_cb = luv_timeout_cb;
	to->data = L;

	uv_set_timeout(loop, to);

	lua_pushvalue(L, 1);
	lua_pushcclosure(L, luv_timeout_cb_inner, 1);
	lua_set_global_callback_and_pushname(L, "timer", to);

	// return timer_xxx
	return 1;
}

static int lua_info(lua_State *L) {
	const char *msg = lua_tostring(L, -1);
	info("%s", msg);
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
		_log(LOG_ERROR, at_func, at_file, at_lineno, "lua_dostring: %s", lua_tostring(L, -1));
		print_trackback();
		exit(-1);
	}
}

void lua_dofile_or_die_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L, char *fname) {
	int r = luaL_loadfile(L, fname);
	if (r) {
		if (r == LUA_ERRFILE) 
			_log(LOG_ERROR, at_func, at_file, at_lineno, "'%s' open failed", fname);
		else if (r == LUA_ERRSYNTAX) 
			_log(LOG_ERROR, at_func, at_file, at_lineno, "'%s' has syntax error", fname);
		else 
			_log(LOG_ERROR, at_func, at_file, at_lineno, "'%s' has unknown error", fname);
		exit(-1);
	}
	lua_call_or_die_at(at_func, at_file, at_lineno, L, 0, LUA_MULTRET);
}

void lua_call_or_die_at(const char *at_func, const char *at_file, int at_lineno, lua_State *L, int nargs, int nresults) {
	if (lua_docall(L, nargs, nresults)) {
		_log(LOG_ERROR, at_func, at_file, at_lineno, "lua_call: %s", lua_tostring(L, -1));
		print_trackback();
		exit(-1);
	}
}

static char tty_readbuf[1024];

static uv_buf_t ttyread_alloc(uv_handle_t *h, size_t len) {
	return uv_buf_init(tty_readbuf, sizeof(tty_readbuf));
}

static void ttyraw_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	if (n <= 0)
		return;

	char key[2] = {};
	*key = *(char *)buf.base;
	if (*key == 'q') {
		info("quits");
		uv_tty_reset_mode();
		exit(0);
	}

	lua_State *L = (lua_State *)st->data;
	lua_pushstring(L, key);
	lua_do_global_callback(L, "ttyraw", st, 1, 0);
}

// ttyraw_open(function (key) 
// end)
static int ttyraw_open(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	uv_tty_t *tty = (uv_tty_t *)zalloc(sizeof(uv_tty_t));
	tty->data = L;

	uv_tty_init(loop, tty, 0, 1);
	uv_tty_set_mode(tty, 1);

	uv_read_start((uv_stream_t *)tty, ttyread_alloc, ttyraw_read);

	lua_set_global_callback(L, "ttyraw", tty);

	return 0;
}

static void stdin_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	lua_State *L = (lua_State *)st->data;

	if (n <= 0)
		return;

	debug("n=%d", n);
	buf.base[n-1] = 0;
	lua_pushstring(L, buf.base);
	lua_do_global_callback(L, "stdin_read", st, 1, 0);
}

// stdin_open(function (line) 
// end)
static int stdin_open(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	uv_tty_t *tty = (uv_tty_t *)zalloc(sizeof(uv_tty_t));
	tty->data = L;

	uv_tty_init(loop, tty, 0, 1);

	uv_read_start((uv_stream_t *)tty, ttyread_alloc, stdin_read);

	lua_set_global_callback(L, "stdin_read", tty);

	return 0;
}


/*
static int prop_get(lua_State *L) {
	return 0;
}

static int prop_set(lua_State *L) {
	return 0;
}
*/

// os.readdir('path', done)
static int os_readdir(lua_State *L) {
	const char *path = lua_tostring(L, 1);

	lua_newtable(L);

	DIR *dir = opendir(path);
	if (dir) {
		struct dirent *e;
		int i;
		for (i = 1; ; i++) {
			e = readdir(dir);
			if (e == NULL)
				break;
			if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) {
				i--;
				continue;
			}
			lua_pushnumber(L, i);
			lua_pushstring(L, e->d_name);
			lua_settable(L, -3);
		}
		closedir(dir);
	}

	return 1;
}

void lua_set_global_ptr(lua_State *L, const char *pref, void *p) {
	char name[128];
	sprintf(name, "%s_%p", pref, p);
	lua_setglobal(L, name);
}

void lua_get_global_ptr(lua_State *L, const char *pref, void *p) {
	char name[128];
	sprintf(name, "%s_%p", pref, p);
	lua_getglobal(L, name);
}

void lua_set_global_callback_and_pushname(lua_State *L, const char *pref, void *p) {
	char name[128];
	sprintf(name, "%s_%p", pref, p);
	lua_setglobal(L, name);
	lua_pushstring(L, name);
}

void lua_set_global_callback(lua_State *L, const char *pref, void *p) {
	lua_set_global_callback_and_pushname(L, pref, p);
	lua_pop(L, 1);
}

void lua_do_global_callback(lua_State *L, const char *pref, void *p, int nargs, int setnil) {
	char name[128];
	sprintf(name, "%s_%p", pref, p);

	lua_getglobal(L, name);
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	if (setnil) {
		lua_pushnil(L);
		lua_setglobal(L, name);
	}

	lua_insert(L, -nargs-1);
	lua_call_or_die(L, nargs, 0);
}

void lua_pushuserptr(lua_State *L, void *p) {
	void *ud = lua_newuserdata(L, sizeof(p));
	memcpy(ud, &p, sizeof(p));
}

void *lua_touserptr(lua_State *L, int index) {
	void *p;
	void *ud = lua_touserdata(L, index);
	memcpy(&p, ud, sizeof(p));
	return p;
}

void lua_setuserptr(lua_State *L, int index, void *p) {
	void *ud = lua_newuserdata(L, sizeof(p));
	memcpy(ud, &p, sizeof(p));
}

static void fault(int sig) {
	fprintf(stderr, "sig %d\n", sig);
	print_trackback();
	exit(-1);
}

void utils_preinit() {
	if (getenv("COREDUMP") == NULL) {
		signal(SIGILL, fault);
		signal(SIGBUS, fault);
		signal(SIGSEGV, fault);
	}

	char *s = getenv("LOG");
	if (s) {
		int v = 1;
		sscanf(s, "%d", &v);
		setloglevel(v);
	}
}

typedef struct {
	char *cmd;
	int ret;
	lua_State *L;
} lua_system_t;

static void lua_system_done(uv_work_t *w, int stat) {
	lua_system_t *s = (lua_system_t *)w->data;

	info("r=%d", s->ret);
	lua_pushnumber(s->L, s->ret);
	lua_do_global_callback(s->L, "system", s, 1, 1);

	free(s->cmd);
	free(s);
	free(w);
}

static void lua_system_thread(uv_work_t *w) {
	lua_system_t *s = (lua_system_t *)w->data;

	info("%s", s->cmd);

	typedef void (*sighandler_t)(int);
  sighandler_t old_handler;
				 
	old_handler = signal(SIGCHLD, SIG_DFL);
	s->ret = system(s->cmd);
	signal(SIGCHLD, old_handler);

	if (s->ret == -1)
		info("ret=%d err=%s", s->ret, strerror(errno));
}

// arg[1] = cmd
// arg[2] = done(code)
static int lua_system(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));
	char *cmd = (char *)lua_tostring(L, 1);

	info("cmd=%s", cmd);

	if (cmd == NULL)
		cmd = "";

	lua_system_t *s = (lua_system_t *)zalloc(sizeof(lua_system_t));
	s->cmd = strdup(cmd);
	s->L = L;

	lua_pushvalue(L, 2);
	lua_set_global_callback(L, "system", s);

	uv_work_t *w = (uv_work_t *)zalloc(sizeof(uv_work_t));
	w->data = s;
	uv_queue_work(loop, w, lua_system_thread, lua_system_done);

	return 0;
}

void utils_init(lua_State *L, uv_loop_t *loop) {
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, luv_set_timeout, 1);
	lua_setglobal(L, "set_timeout");

	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, ttyraw_open, 1);
	lua_setglobal(L, "ttyraw_open");

	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, stdin_open, 1);
	lua_setglobal(L, "stdin_open");

	// os.readdir = [native function]
	lua_getglobal(L, "os");
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, os_readdir, 1);
	lua_setfield(L, -2, "readdir");
	lua_pop(L, 1);

	lua_pushcfunction(L, lua_info);
	lua_setglobal(L, "_info");

	lua_pushcfunction(L, lua_setloglevel);
	lua_setglobal(L, "setloglevel");

	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_system, 1);
	lua_setglobal(L, "system");
}

