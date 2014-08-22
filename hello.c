
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>

#include "utils.h"
#include "strbuf.h"

#include "upnp_device.h"
#include "upnp_util.h"

#include "avconv.h"
#include "audio_out.h"
#include "audio_out_test.h"

typedef struct {
	strbuf_t *sb;
} proc_t;

static uv_buf_t read_alloc_buffer(uv_handle_t *h, size_t len) {
	proc_t *p = (proc_t *)h->data;
	strbuf_ensure_empty_length(p->sb, len);
	return uv_buf_init(p->sb->buf, len);
}

static void pipe_read(uv_stream_t *st, ssize_t nread, uv_buf_t buf) {
	proc_t *p = (proc_t *)((uv_handle_t *)st)->data;
	info("n=%d", nread);
	if (nread > 0) {
		int end = 0;
		char *s = (char *)buf.base;
		int len = buf.len;
		while (len-- && *s) {
			if (*s == '\n') { end = 1; break; }
			s++;
		}
		if (end) {
			info("end");
		}
		strbuf_append_mem(p->sb, buf.base, buf.len);
	}
}

static void proc_on_exit(uv_process_t *puv, int stat, int sig) {
	proc_t *p = (proc_t *)puv->data;
	
	strbuf_append_char(p->sb, '\x00');
	info("%s", p->sb->buf);

	strbuf_free(p->sb);
	free(p);
}

static void shutdown_done(uv_shutdown_t *s, int stat) {
	info("done");
}

static void pipe_write_done(uv_write_t *w, int stat) {
	uv_shutdown_t *s = malloc(sizeof(uv_shutdown_t));
	uv_shutdown(s, w->handle, shutdown_done);
}

static void test_uv_subprocess(int hello) {
	info("starts");

	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	uv_loop_t *loop = uv_loop_new();

	proc_t *p = (proc_t *)malloc(sizeof(proc_t));
	p->sb = strbuf_new(4096);

	uv_process_t *puv = (uv_process_t *)zalloc(sizeof(uv_process_t));
	puv->data = p;

	int i;
	uv_pipe_t pipe[3] = {};
	for (i = 0; i < 3; i++) {
		uv_pipe_init(loop, &pipe[i], 0);
		uv_pipe_open(&pipe[i], 0);
		pipe[i].data = p;
	}

	uv_process_options_t opts = {};

	uv_stdio_container_t stdio_test_echo_fd[16] = {};
	for (i = 0; i < 16; i++) {
		uv_stdio_container_t c = { .flags = UV_IGNORE };
		memcpy(&stdio_test_echo_fd[i], &c, sizeof(c));
	}
	{
		uv_stdio_container_t c = {.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)&pipe[0]};
		memcpy(&stdio_test_echo_fd[9], &c, sizeof(c));
	}

	uv_stdio_container_t stdio_test_cat[3] = {
		{.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)&pipe[0]},
		{.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)&pipe[1]},
		{.flags = UV_IGNORE},
	};

	char *args_test_cat[] = {"cat", NULL};
	char *args_test_echo_fd[] = {"sh", "-c", "echo {1:3,4:3} >&9; sleep 1000", NULL};
	char **args;

	if (hello == 7)
		args = args_test_cat;
	if (hello == 8)
		args = args_test_echo_fd;

	opts.file = args[0];
	opts.args = args;

	if (hello == 7) {
		opts.stdio_count = 3;
		opts.stdio = stdio_test_cat;
	}
	if (hello == 8) {
		opts.stdio_count = 16;
		opts.stdio = stdio_test_echo_fd;
	}

	opts.exit_cb = proc_on_exit;

	int r = uv_spawn(loop, puv, opts);
	info("spawn=%d", r);

	if (hello == 7) {
		uv_write_t w = { .data = &pipe[1] };
		uv_buf_t buf = { .base = "hello world", .len = 12 };
		uv_write(&w, (uv_stream_t *)&pipe[0], &buf, 1, pipe_write_done);
		uv_read_start((uv_stream_t *)&pipe[1], read_alloc_buffer, pipe_read);
	}
	if (hello == 8) {
		uv_read_start((uv_stream_t *)&pipe[0], read_alloc_buffer, pipe_read);
	}

	uv_run(loop, UV_RUN_DEFAULT);
	info("exits");
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
}

static void test_pthread_call_luv() {
	uv_loop_t *loop = uv_loop_new();
	uv_async_t as;
	uv_async_init(loop, &as, NULL);

	pthread_t tid;
	pthread_create(&tid, NULL, pthread_loop_1, loop);

	/*
	pthread_call_luv_sync(loop, on_done, (void *)1);

	pthread_call_luv_async(loop, func (L, done) {
		emit(done)
	}, on_done, (void *)1);
	*/

	int i;
	for (i = 0; i < 1000000; i++) 
		pthread_call_luv_sync(NULL, loop, pcall_luv_cb, &i);
}

static int buggy_func(lua_State *L) {
	lua_pushnumber(L, 11);
	lua_pushnumber(L, 11);
	lua_pushnumber(L, 11);
	lua_pushnumber(L, 11);
	lua_pushnumber(L, 11);
	lua_pushnumber(L, 11);
	lua_pushnumber(L, 11);
	lua_pushnumber(L, 11);
	return 0;
}

static void test_buggy_call() {
	info("starts");

	lua_State *L = luaL_newstate();
	luaL_openlibs(L);

	int i;
	for (i = 0; i < 100; i++) {
		lua_pushcfunction(L, buggy_func);
		lua_call(L, 0, 0);
		luaL_dostring(L, "print(1,2,3)");
	}
}

static void avconv_read_done(avconv_t *av, int nread) {
	info("n=%d", nread);

	if (nread < 0) {
		info("EOF");
		return;
	}

	avconv_read(av, av->data, 1024*1024, avconv_read_done);
}

void test_avconv(uv_loop_t *loop) {
	avconv_t *av = (avconv_t *)zalloc(sizeof(avconv_t));

	av->data = malloc(1024*1024*1);

	avconv_start(loop, av, "testdata/test.mp3");
	avconv_read(av, av->data, 1024*1024, avconv_read_done);
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
	if (i == 6)
		test_buggy_call();
	if (i == 7 || i == 8)
		test_uv_subprocess(i);
}

typedef struct {
	avconv_t *av;
	audio_out_t *ao;
	void *buf;
	int len;
} avconv_ao_test_t;

static void avconv_audio_out_on_data(avconv_t *av, int nread);
static void avconv_audio_out_play_done(audio_out_t *ao);

static void avconv_audio_out_play_done(audio_out_t *ao) {
	avconv_ao_test_t *t = (avconv_ao_test_t *)ao->data;

	avconv_read(t->av, t->buf, t->len, avconv_audio_out_on_data);
}

static void avconv_audio_out_on_data(avconv_t *av, int nread) {
	avconv_ao_test_t *t = (avconv_ao_test_t *)av->data;
	if (nread < 0)
		return;
	audio_out_play(t->ao, t->buf, t->len, avconv_audio_out_play_done);
}

static void test_avconv_audio_out(uv_loop_t *loop) {
	avconv_t *av = (avconv_t *)zalloc(sizeof(avconv_t));
	audio_out_t *ao = (audio_out_t *)zalloc(sizeof(audio_out_t));
	avconv_ao_test_t *t = (avconv_ao_test_t *)zalloc(sizeof(avconv_ao_test_t));

	audio_out_init(loop, ao, 44100);

	t->ao = ao;
	t->av = av;

	av->data = t;
	ao->data = t;

	t->len = 4096;
	t->buf = malloc(t->len);

	avconv_start(loop, av, "testdata/test.mp3");
	avconv_read(av, t->buf, t->len, avconv_audio_out_on_data);
}

void run_test_c(int i, lua_State *L, uv_loop_t *loop) {
	info("i=%d", i);
	if (i == 1)
		test_avconv(loop);
	if (i == 2)
		test_avconv_audio_out(loop);
	if (i == 3)
		test_audio_out(loop);
	if (i == 4)
		test_avconv_audio_out(loop);
}

void run_test_lua(int i, lua_State *L, uv_loop_t *loop) {
	char name[64];
	sprintf(name, "test_%d.lua", i);
	info("dofile %s", name);
	lua_dofile_or_die(L, name);
}

