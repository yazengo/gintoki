
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "utils.h"
#include "strbuf.h"
#include "ringbuf.h"

#include "upnp_device.h"
#include "upnp_util.h"

#include "lua_cjson.h"
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

typedef struct {
	lua_State *L;
	uv_loop_t *loop;
} test_pcall_v2_t;

// arg[1] = data
// arg[2] = done function ret value
static int pcall_luv_v2_done_cb(lua_State *L) {
	int i = lua_tonumber(L, 2);

	info("done %d", i);

	return 0;
}

// arg[1] = data
// arg[2] = done function
static int pcall_luv_v2_start_cb(lua_State *L) {
	
	info("%p", lua_touserptr(L, 1));

	int i = *(int *)lua_touserptr(L, 1);

	lua_getglobal(L, "test_pcall");
	lua_pushvalue(L, 2);
	lua_pushnumber(L, i);
	lua_call_or_die(L, 2, 0);

	return 0;
}

static void *pthread_loop_test_pcall_v2(void *_p) {
	test_pcall_v2_t *t = (test_pcall_v2_t *)_p;

	int i;
	for (i = 0; i < 10000; i++) {
		info("calling %d", i);
		pthread_call_luv_sync_v2(t->L, t->loop, pcall_luv_v2_start_cb, pcall_luv_v2_done_cb, &i);
	}

	return NULL;
}

static void test_pthread_call_luv_v2(lua_State *L, uv_loop_t *loop) {
	lua_dostring_or_die(L,
		"test_pcall = function (done, i) \n"
		"  set_timeout(function ()       \n"
		"    info('call', i)             \n"
		"    done(i)                     \n"
		"  end, 1)                       \n"
		"end                             \n"
	);

	test_pcall_v2_t *t = (test_pcall_v2_t *)zalloc(sizeof(test_pcall_v2_t));
	t->loop = loop;
	t->L = L;

	pthread_t tid;
	pthread_create(&tid, NULL, pthread_loop_test_pcall_v2, t);
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
	for (i = 0; i < 100000; i++) {
		lua_pushcfunction(L, buggy_func);
		lua_call(L, 0, 0);
		luaL_dostring(L, "print(1,2,3)");
	}
}

static void ringbuf_get_done(ringbuf_t *rb, int len) {
	info("get %d", len);
}

static void test_ringbuf(uv_loop_t *loop) {
	ringbuf_t rb;
	static char buf[176400];

	ringbuf_init(&rb, loop);

	ringbuf_data_get(&rb, buf, 24576, ringbuf_get_done);
	ringbuf_data_put(&rb, buf, sizeof(buf), NULL);
	ringbuf_data_get(&rb, buf, 2048, ringbuf_get_done);
}

void run_test_c_pre(int i) {
	info("run hello world #%d", i);
	if (i == 1)
		test_uv_send_async_before_run_loop();
	if (i == 2)
		test_lua_call_c();
	if (i == 3)
		test_lua_cjson();
	if (i == 4)
		test_work_queue();
	if (i == 6)
		test_buggy_call();
	if (i == 7 || i == 8)
		test_uv_subprocess(i);
}

static uv_buf_t uv_malloc_buffer(uv_handle_t *h, size_t len) {
	return uv_buf_init(malloc(len), len);
}

static void tty_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	info("n=%d", n);
	if (n < 0)
		return;

	info("key=%c", *(char *)buf.base);

	while (n--) {
		if (*(char *)buf.base == 'q') {
			uv_tty_reset_mode();
			exit(-1);
		}
		buf.base++;
	}
}

static void test_ttyraw_open(uv_loop_t *loop) {
	uv_tty_t *tty = (uv_tty_t *)zalloc(sizeof(uv_tty_t));

	uv_tty_init(loop, tty, 0, 1);
	uv_tty_set_mode(tty, 1);

	uv_read_start((uv_stream_t *)tty, uv_malloc_buffer, tty_read);
}

#if 0
typedef struct {
	lua_State *L;
	uv_loop_t *loop;
} luv_tcp_t;

static void luv_tcp_alloc(uv_handle_t *h, size_t size, uv_buf_t* buf) {
	buf->base = malloc(size);
	buf->len = size;
}

static void luv_tcp_alloc(uv_handle_t *h, size_t size, uv_buf_t* buf) {
	buf->base = malloc(size);
	buf->len = size;
}

static void luv_tcp_read(uv_stream_t *h, ssize_t n, const uv_buf_t* buf) {
	int i;
	write_req_t *wr;

	if (n < 0) {
		if (buf->base) {
			free(buf->base);
		}
		uv_close((uv_handle_t *)h, luv_on_close);
		return;
	}

	if (nread == 0) {
		free(buf->base);
		return;
	}

	if (!server_closed) {
		for (i = 0; i < nread; i++) {
			if (buf->base[i] == 'Q') {
				if (i + 1 < nread && buf->base[i + 1] == 'S') {
					free(buf->base);
					uv_close((uv_handle_t*)handle, on_close);
					return;
				} else {
					uv_close(server, on_server_close);
					server_closed = 1;
				}
			}
		}
	}

	wr = (write_req_t*) malloc(sizeof *wr);
	ASSERT(wr != NULL);
	wr->buf = uv_buf_init(buf->base, nread);

	if (uv_write(&wr->req, handle, &wr->buf, 1, after_write)) {
		FATAL("uv_write failed");
	}
}



static void on_connection(uv_stream_t *srv, int status) {
	luv_tcp_t *tcp = (luv_tcp_t *)src->data;

	uv_stream_t *cli = (uv_stream_t *)zalloc(sizeof(uv_tcp_t));
	cli->data = srv->data;
	uv_tcp_init(loop, (uv_tcp_t *)cli);
	uv_accept(srv, cli);
	uv_read_start(cli, );
}

static void test_tcp(uv_loop_t *loop) {
	luv_tcp_t *tcp = (luv_tcp_t *)zalloc(sizeof(luv_tcp_t));
	tcp->loop = loop;

	uv_tcp_t *srv = (uv_tcp_t *)zalloc(sizeof(uv_tcp_t));
	srv->data = tcp;
	uv_tcp_init(loop, srv);
	struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", 7000);
	uv_tcp_bind(srv, (const struct sockaddr *)&addr, 0);
	uv_listen((uv_stream_t *)srv, SOMAXCONN, on_connection);
}

#endif

static void pcall_uv_sleep(void *pcall, void *_) {
	sleep(1);
	pthread_call_uv_complete(pcall);
}

static void *pthread_loop_test_pcall_uv(void *_p) {
	uv_loop_t *loop = (uv_loop_t *)_p;

	int i;
	for (i = 0; i < 20; i++) {
		pthread_call_uv_wait(loop, pcall_uv_sleep, NULL);
		info("%d", i);
	}
	return NULL;
}

static void test_pthread_call_uv(lua_State *L, uv_loop_t *loop) {
	pthread_t tid;
	pthread_create(&tid, NULL, pthread_loop_test_pcall_uv, loop);
}

static void tty_stdin_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	info("n=%d", n);
	if (n < 0)
		return;

	info("%s", buf.base);
}

static void test_stdin(uv_loop_t *loop) {
	info("test tty stdin");

	uv_tty_t *tty = (uv_tty_t *)zalloc(sizeof(uv_tty_t));

	uv_tty_init(loop, tty, 0, 1);

	uv_read_start((uv_stream_t *)tty, uv_malloc_buffer, tty_stdin_read);
}

void run_test_c_post(int i, lua_State *L, uv_loop_t *loop) {
	info("i=%d", i);
	if (i == 3)
		test_audio_out(loop);
	if (i == 5) 
		test_ttyraw_open(loop);
	if (i == 6)
		test_pthread_call_luv_v2(L, loop);
	if (i == 7)
		test_pthread_call_uv(L, loop);
	if (i == 8)
		test_ringbuf(loop);
	if (i == 9)
		test_stdin(loop);
}

