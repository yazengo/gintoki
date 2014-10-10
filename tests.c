
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "utils.h"
#include "strbuf.h"
#include "strparser.h"
#include "ringbuf.h"
#include "luv_curl.h"
#include "blowfish.h"

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

static void test_blowfish() {
  uint32_t L = 1, R = 2;
  BLOWFISH_CTX ctx;

  Blowfish_Init (&ctx, (uint8_t *)"TESTKEY", 7);
  Blowfish_Encrypt(&ctx, &L, &R);
  printf("%08X %08X\n", L, R);
  if (L == 0xDF333FD2L && R == 0x30A71BB4L)
	  info("Test encryption OK.");
  else
	  info("Test encryption failed.");
  Blowfish_Decrypt(&ctx, &L, &R);
  if (L == 1 && R == 2)
  	  info("Test decryption OK.");
  else
	  info("Test decryption failed.");

	blowfish_t *b = (blowfish_t *)zalloc(sizeof(blowfish_t));

	char *key = "6#26FRL$ZWD";
	blowfish_init(b, key, strlen(key));

	char in[32];
	char out[64];
	char decode_out[64];

	memset(in, 0, sizeof(in));
	strcpy(in, "abcdefgh");

	memset(out, 0, sizeof(out));
	blowfish_encode_hex(b, in, 8, out);
	info("encode_out: %s", out);

	memset(decode_out, 0, sizeof(decode_out));
	blowfish_decode_hex(b, out, 8*2, decode_out);
	info("decode_out: %s", decode_out);
}

typedef struct {
	int type, len;
} cmdhdr_t;


static void write2(void *buf, int len) {
	while (len > 0) {
		int r = write(4, buf, len);
		if (r < 0)
			break;
		len -= r;
		buf += r;
	}
}

static void test_fake_shairport() {
	info("starts");

#define step 44100
#define n 3
#define repeat 3

	static char buf[step];
	int key = 0;
	int i, r;

	for (r = 0; r < repeat; r++) {
		cmdhdr_t c = {};

		c.type = 0;
		write2(&c, sizeof(c));

		for (i = 0; i < n; i++) {
			audio_out_test_fill_buf_with_key(buf, sizeof(buf), 44100, key);

			cmdhdr_t c = {.type = 1, .len = sizeof(buf)};
			write2(&c, sizeof(c));
			write2(buf, sizeof(buf));

			key = (key+1)%7;
		}

		c.type = 2;
		write2(&c, sizeof(c));
	}

#undef step
#undef n
#undef repeat
}

void run_test_c_pre(int i) {
	info("run C pre test #%d", i);
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
	if (i == 9)
		test_blowfish();
	if (i == 10)
		test_fake_shairport();
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

typedef struct {
	uv_loop_t *loop;

	uv_fs_t *req_open;
	uv_fs_t *req_read;
	uv_fs_t *req_stat;
	char *fname_tmp;
	char *fname_done;
	int stat;
	uv_file fd_tmp;
	uv_file fd_done;
	int tmp_closed;
	uv_timer_t *timer;

	char readbuf[2048];
	char *writebuf;
	int writelen;

	int64_t off, size;
	uv_fs_poll_t *poll;
} avconv_tail_t;

enum {
	TAIL_OPENING_TMP,
	TAIL_READING_TMP,
	TAIL_WRITING_STDIN_TMP,
	TAIL_POLLING_TMP,
	TAIL_OPENING_DONE,
	TAIL_READING_DONE,
	TAIL_WRITING_STDIN_DONE,
};

static void tail_on_read(uv_fs_t *req);
static void tail_open_done(avconv_tail_t *tl);
static void tail_on_poll(uv_fs_poll_t *h, int stat, const uv_statbuf_t *prev, const uv_statbuf_t *curr);

static void tail_close(avconv_tail_t *tl) {
}

static void tail_close_stdin(avconv_tail_t *tl) {
	debug("close stdin");
}

static void tail_poll(avconv_tail_t *tl) {
	debug("poll starts: %s", tl->fname_tmp);

	tl->poll = zalloc(sizeof(uv_fs_poll_t));
	tl->poll->data = tl;
	uv_fs_poll_init(tl->loop, tl->poll);
	uv_fs_poll_start(tl->poll, tail_on_poll, tl->fname_tmp, 1000);
}

static void tail_on_write_stdin_done(avconv_tail_t *tl) {
	uv_file fd;

	if (tl->stat == TAIL_WRITING_STDIN_TMP) {
		tl->stat = TAIL_READING_TMP;
		fd = tl->fd_tmp;
	} else if (tl->stat == TAIL_WRITING_STDIN_DONE) {
		tl->stat = TAIL_READING_DONE;
		fd = tl->fd_done;
	} else
		panic("stat error");
	uv_fs_read(tl->loop, tl->req_read, fd, tl->readbuf, sizeof(tl->readbuf), tl->off, tail_on_read);
}

static void tail_write_stdin(avconv_tail_t *tl, char *buf, int len) {
	tl->writebuf = buf;
	tl->writelen = len;

	tail_on_write_stdin_done(tl);
}

static void tail_on_read(uv_fs_t *req) {
	avconv_tail_t *tl = req->data;
	uv_fs_req_cleanup(req);

	debug("off=%lld read=%d", tl->off, req->result);

	if (req->result <= 0) {
		if (tl->stat == TAIL_READING_TMP) {
			tl->stat = TAIL_POLLING_TMP;
			tail_poll(tl);
		} else if (tl->stat == TAIL_READING_DONE) {
			tail_close_stdin(tl);
		} else
			panic("stat error");
		return;
	}

	tl->off += req->result;

	if (tl->stat == TAIL_READING_TMP)
		tl->stat = TAIL_WRITING_STDIN_TMP;
	else if (tl->stat == TAIL_READING_DONE)
		tl->stat = TAIL_WRITING_STDIN_DONE;
	else
		panic("stat error");

	tail_write_stdin(tl, tl->readbuf, req->result);
}

static void tail_on_poll(uv_fs_poll_t *h, int stat, const uv_statbuf_t *prev, const uv_statbuf_t *curr) {
	avconv_tail_t *tl = h->data;

	uv_fs_poll_stop(tl->poll);

	debug("stat=%d", stat);
	if (stat == -1) {
		tl->stat = TAIL_OPENING_DONE;
		tail_open_done(tl);
		return;
	}

	tl->size = curr->st_size;
	debug("size=%lld", tl->size);

	tl->stat = TAIL_READING_TMP;
	uv_fs_read(tl->loop, tl->req_read, tl->fd_tmp, tl->readbuf, sizeof(tl->readbuf), tl->off, tail_on_read);
}

static void tail_on_open_done(uv_fs_t *req) {
	avconv_tail_t *tl = req->data;
	uv_fs_req_cleanup(req);

	debug("r=%d", req->result);
	if (req->result < 0) {
		tail_close(tl);
		return;
	}

	tl->stat = TAIL_READING_DONE;
	tl->fd_done = req->result;
	uv_fs_read(tl->loop, tl->req_read, tl->fd_done, tl->readbuf, sizeof(tl->readbuf), tl->off, tail_on_read);
}

static void tail_open_done(avconv_tail_t *tl) {
	tl->stat = TAIL_OPENING_DONE;
	uv_fs_open(tl->loop, tl->req_open, tl->fname_done, O_RDONLY, 0, tail_on_open_done);
}

static void tail_on_open_tmp(uv_fs_t *req) {
	avconv_tail_t *tl = req->data;
	uv_fs_req_cleanup(req);

	debug("r=%d", req->result);
	if (req->result < 0) {
		tail_open_done(tl);
		return;
	}

	tl->stat = TAIL_READING_TMP;
	tl->fd_tmp = req->result;
	uv_fs_read(tl->loop, tl->req_read, tl->fd_tmp, tl->readbuf, sizeof(tl->readbuf), tl->off, tail_on_read);
}

static void test_poll1(uv_loop_t *loop) {
	setloglevel(0);

	avconv_tail_t *tl = zalloc(sizeof(avconv_tail_t));
	tl->fname_tmp = "/tmp/change.tmp";
	tl->fname_done = "/tmp/change";

	tl->req_read = zalloc(sizeof(uv_fs_t));
	tl->req_read->data = tl;
	tl->req_open = zalloc(sizeof(uv_fs_t));
	tl->req_open->data = tl;

	tl->stat = TAIL_OPENING_TMP;
	tl->loop = loop;

	uv_fs_open(loop, tl->req_open, tl->fname_tmp, O_RDONLY, 0, tail_on_open_tmp);
}

static void test_panic() {
	panic("BOOM");
}

void run_test_c_post(int i, lua_State *L, uv_loop_t *loop, char **argv) {
	info("i=%d", i);
	if (i == 3)
		test_audio_out(loop);
	if (i == 5) 
		test_ttyraw_open(loop);
	if (i == 6)
		test_pthread_call_luv_v2(L, loop);
	if (i == 7)
		test_pthread_call_uv(L, loop);
	if (i == 9)
		test_stdin(loop);
	if (i == 10)
		test_poll1(loop);
	if (i == 12)
		test_panic();
}

