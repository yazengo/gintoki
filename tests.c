
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "luv.h"
#include "utils.h"
#include "strbuf.h"
#include "curl.h"
#include "blowfish.h"

#include "cjson.h"

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

static void luv_lobj_gc(uv_loop_t *loop, void *_p) {
	info("called");
}

static int luv_lobj(lua_State *L, uv_loop_t *loop) {
	int *p = (int *)luv_newctx(L, loop, sizeof(int));
	luv_setgc(p, luv_lobj_gc);
	luv_unref(p);
	return 1;
}

static void threadtest_done(uv_work_t *w, int stat) {
	lua_State *L = luv_state(w);

	luv_pushctx(L, w);
	lua_getfield(L, -1, "done");
	luv_xmove(luv_threadstate(w), L, 1);
	lua_call_or_die(L, 1, 0);
}

static void threadtest_run(uv_work_t *w) {
	lua_State *L = luv_threadstate(w);

	lua_dostring_or_die(L, "r = {"
		"1,2,3.14159,c={'c','d'},d={e={f=1,g=2}, k={c=3}},"
		"function () end"
	"}");
	lua_getglobal(L, "r");
}

static int luv_threadtest(lua_State *L, uv_loop_t *loop) {
	uv_work_t *w = (uv_work_t *)luv_newthreadctx(L, loop, sizeof(uv_work_t));
	lua_pushvalue(L, 1);
	lua_setfield(L, -2, "done");
	uv_queue_work(loop, w, threadtest_run, threadtest_done);
	return 1;
}

static void test_luv(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "lobj", luv_lobj);
	lua_dostring_or_die(L, "lobj()");

	luv_register(L, loop, "threadtest", luv_threadtest);
	lua_dostring_or_die(L, "threadtest(function (r) info(r) end)");
}

static void fsopen_done(fs_req_t *req) {
	info("fd=%d", req->fd);
	fs_req_cleanup(req);
	free(req);
}

static void test_fsopen(uv_loop_t *loop) {
	fs_req_t *req = (fs_req_t *)zalloc(sizeof(fs_req_t));
	req->path = strdup("/dev/null");
	info("open %s", req->path);
	fs_open(loop, req, fsopen_done);
}

void run_test(int i, lua_State *L, uv_loop_t *loop, char **argv) {
	info("i=%d", i);
	if (i == 1)
		test_luv(L, loop);
	if (i == 2)
		test_blowfish();
	if (i == 3)
		test_fsopen(loop);
}

