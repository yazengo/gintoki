
#include <dirent.h>
#include <unistd.h>

#include "luv.h"
#include "utils.h"

static int luv_now(lua_State *L, uv_loop_t *loop) {
	lua_pushnumber(L, now());
	return 1;
}

static int luv_hostname(lua_State *L, uv_loop_t *loop) {
	char name[512] = {};
	gethostname(name, sizeof(name)-1);
	lua_pushstring(L, name);
	return 1;
}

typedef struct {
	uv_tty_t tty;
	char buf[2048];
} readline_t;

static uv_buf_t readline_allocbuf(uv_handle_t *h, size_t len) {
	readline_t *rl = (readline_t *)h->data;
	return uv_buf_init(rl->buf, sizeof(rl->buf));
}

static void readline_on_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	readline_t *rl = (readline_t *)st->data;

	if (n <= 0)
		return;
	buf.base[n-1] = 0;

	lua_State *L = luv_state(rl);
	int t = lua_gettop(L);

	luv_pushctx(L, rl);
	lua_getfield(L, t+1, "_read");
	lua_pushstring(L, buf.base);
	lua_call_or_die(L, 1, 0);

	lua_settop(L, t);
}

// readline(func)
static int luv_readline(lua_State *L, uv_loop_t *loop) {
	readline_t *rl = (readline_t *)luv_newctx(L, loop, sizeof(readline_t));

	rl->tty.data = rl;
	uv_tty_init(loop, &rl->tty, 0, 1);

	// set tty in raw mode can use
	// uv_tty_set_mode(tty, 1);

	uv_read_start((uv_stream_t *)&rl->tty, readline_allocbuf, readline_on_read);

	lua_pushvalue(L, 1);
	lua_setfield(L, -2, "_read");

	return 0;
}

static void readdir_done(uv_work_t *w, int stat) {
	lua_State *L = luv_state(w);
	int i = lua_gettop(L);

	lua_xmove(luv_threadstate(w), L, 1);
	lua_getfield(L, i+1, "done");
	lua_getfield(L, i+1, "_r");
	lua_call_or_die(L, 1, 0);

	lua_settop(L, i);
	luv_unref(w);
}

static void readdir_thread(uv_work_t *w) {
	lua_State *L = luv_threadstate(w);

	lua_getfield(L, 1, "path");
	const char *path = lua_tostring(L, -1);
	
	lua_newtable(L);
	int t = lua_gettop(L);

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
			lua_settable(L, t);
		}
		closedir(dir);
	}
	lua_setfield(L, 1, "_r");

	lua_pushvalue(L, 1);
}

static int luv_readdir(lua_State *L, uv_loop_t *loop) {
	uv_work_t *w = (uv_work_t *)luv_newthreadctx(L, loop, sizeof(uv_work_t));
	lua_pushvalue(L, 1);
	lua_xmove(L, luv_threadstate(w), 1);
	uv_queue_work(loop, w, readdir_thread, readdir_done);
	return 0;
}

void luv_os_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "readdir", luv_readdir);
	luv_register(L, loop, "readline", luv_readline);
	luv_register(L, loop, "hostname", luv_hostname);
	luv_register(L, loop, "now", luv_now);
}

