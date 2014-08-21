
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>

#include "utils.h"
#include "strbuf.h"
#include "upnp_device.h"
#include "upnp_util.h"

static int volup(lua_State *L) {
	info("call");
	lua_pushnumber(L, 1);
	return 1;
}

typedef struct {
	strbuf_t *sb;
} proc_t;

static uv_buf_t alloc_buffer(uv_handle_t *h, size_t len) {
	proc_t *p = (proc_t *)h->data;
	strbuf_ensure_empty_length(p->sb, len);
	return uv_buf_init(p->sb->buf, len);
}

static void pipe_read(uv_stream_t *st, ssize_t nread, uv_buf_t buf) {
	proc_t *p = (proc_t *)((uv_handle_t *)st)->data;
	if (nread > 0)
		strbuf_append_mem(p->sb, buf.base, buf.len);
}

static void proc_on_exit(uv_process_t *puv, int stat, int sig) {
	proc_t *p = (proc_t *)puv->data;
	
	strbuf_append_char(p->sb, '\x00');
	info("%s", p->sb->buf);

	strbuf_free(p->sb);
	free(p);
}

static void test_uv_subprocess() {
	info("starts");

	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	lua_register(L, "volup", volup);
	lua_getglobal(L, "volup");
	lua_pcall(L, 0, 1, 0);

	uv_loop_t *loop = uv_loop_new();

	uv_pipe_t pipe = {};
	uv_pipe_init(loop, &pipe, 0);
	uv_pipe_open(&pipe, 0);

	proc_t *p = (proc_t *)malloc(sizeof(proc_t));
	p->sb = strbuf_new(4096);

	uv_process_t *puv = (uv_process_t *)malloc(sizeof(uv_process_t));
	memset(puv, 0, sizeof(uv_process_t));

	puv->data = p;
	pipe.data = p;

	uv_process_options_t opts = {};
	uv_stdio_container_t stdio[3] = {
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE|UV_READABLE_PIPE, .data.stream = (uv_stream_t *)&pipe},
		{.flags = UV_IGNORE},
	};

	char *args[] = {"echo", "hello world", NULL};
	opts.file = args[0];
	opts.args = args;
	opts.stdio = stdio;
	opts.stdio_count = 3;
	opts.exit_cb = proc_on_exit;

	int r = uv_spawn(loop, puv, opts);
	info("spawn=%d", r);

	uv_read_start((uv_stream_t *)&pipe, alloc_buffer, pipe_read);

	uv_run(loop, UV_RUN_DEFAULT);
}

static void send_async_before(uv_async_t *h, int stat) {
	info("runs");
}

static void test_uv_send_async_before_run_loop() {
	uv_loop_t *loop = uv_loop_new();

	uv_async_t as;
	uv_async_init(loop, &as, send_async_before);
	uv_async_send(&as);

	sleep(2);
	info("start");

	uv_run(loop, UV_RUN_DEFAULT);
}

/*
emitter.init(upnp)
upnp.on('subscribe', function (t) 
end)
upnp.emit('subscribe', {})
upnp.accept({})
upnp.notify({})
*/
static int upnp_accept(lua_State *L) {
	lua_getfield(L, -2, "msg");
	const char *msg = lua_tostring(L, -1);
	lua_remove(L, -1);

	info("msg=%s", msg);

	return 0;
}

static void test_lua_call_c() {
	lua_State *L = luaL_newstate();

	luaL_openlibs(L);
	luaL_dofile(L, "utils.lua");

	// upnp = {}
	lua_newtable(L);
	lua_setglobal(L, "upnp");

	// emitter_init(upnp)
	lua_getglobal(L, "emitter_init");
	lua_getglobal(L, "upnp");
	lua_call(L, 1, 0);

	// upnp.accept = ...
	lua_getglobal(L, "upnp");
	lua_pushcfunction(L, upnp_accept);
	lua_setfield(L, -2, "accept");
	lua_remove(L, -1);

	luaL_dostring(L, "upnp.on('hello', function (a, b) print('hello', a, b); upnp.accept({msg='hi'}, 12) end)");

	// upnp.emit('hello', 1, 2)
	lua_getglobal(L, "upnp");
	lua_getfield(L, -1, "emit");
	lua_remove(L, -2);
	lua_pushstring(L, "hello");
	lua_pushinteger(L, 1);
	lua_pushstring(L, NULL);
	lua_call(L, 3, 0);
}

static void test_lua_cjson() {
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_cjson_safe(L);
	luaL_dostring(L, "print(cjson.encode({a=1,b=2,{}}))");
	luaL_dostring(L, "for k,v in pairs(cjson.decode('{\"a\":1,\"b\":\"2\"}')) do print (k,v) end");
	luaL_dostring(L, "for k,v in pairs(cjson.decode('{\"a\":1,\"b\":\"2\"}')) do print (k,v) end");
}

static void fib(uv_work_t *w) {
	info("calls");
	sleep(1);
	info("end");
}

static void test_work_queue() {
	uv_loop_t *loop = uv_loop_new();
	uv_work_t req[3];
	int i;

	for (i = 0; i < 3; i++) {
		uv_queue_work(loop, &req[i], fib, NULL);
	}

	uv_run(loop, UV_RUN_DEFAULT);
}

static void *pthread_loop_1(void *_) {
	uv_loop_t *loop = (uv_loop_t *)_;
	uv_run(loop, UV_RUN_DEFAULT);
	return NULL;
}

static void pcall_luv_cb(lua_State *L, void *_) {
	info("%p", _);
	sleep(1);
}

static void test_pthread_call_luv() {
	uv_loop_t *loop = uv_loop_new();
	uv_async_t as;
	uv_async_init(loop, &as, NULL);

	pthread_t tid;
	pthread_create(&tid, NULL, pthread_loop_1, loop);

	pthread_call_luv_sync(NULL, loop, pcall_luv_cb, (void *)1);
	pthread_call_luv_sync(NULL, loop, pcall_luv_cb, (void *)2);
	pthread_call_luv_sync(NULL, loop, pcall_luv_cb, (void *)3);
}

void run_hello(int i) {
	info("run hello world #%d", i);
	if (i == 1)
		test_uv_send_async_before_run_loop();
	if (i == 2)
		test_lua_call_c();
	if (i == 3)
		test_lua_cjson();
	if (i == 4)
		test_work_queue();
	if (i == 5)
		test_pthread_call_luv();
}

