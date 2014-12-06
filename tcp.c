
#include <stdlib.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"

static void on_closed(uv_handle_t *t) {
	free(t);
}

static void gc(pipe_t *p) {
	debug("p=%p", p);
	free(p->st);
}

static void on_conn(uv_stream_t *st, int stat) {
	uv_tcp_t *t = (uv_tcp_t *)zalloc(sizeof(uv_tcp_t));
	uv_tcp_init(st->loop, t);

	if (uv_accept(st, (uv_stream_t *)t)) {
		warn("accept failed");
		uv_close((uv_handle_t *)t, on_closed);
		return;
	}

	pipe_t *p = pipe_new(luv_state(st), st->loop);
	p->type = PSTREAM_SRC;
	p->st = (uv_stream_t *)t;
	p->gc = gc;
	debug("p=%p", p);

	pipe_t *po = pipe_new(luv_state(st), st->loop);
	po->type = PSTREAM_SINK;
	po->st = (uv_stream_t *)t;

	luv_callfield(st, "conn_cb", 2, 0);
}

// tcpsrv(addr, port, function (r, w) end)
static int luv_tcpsrv(lua_State *L, uv_loop_t *loop) {
	char *addr = (char *)lua_tostring(L, 1);
	int port = lua_tonumber(L, 2);

	if (addr == NULL)
		addr = "0.0.0.0";

	uv_tcp_t *t = (uv_tcp_t *)luv_newctx(L, loop, sizeof(uv_tcp_t));
	uv_tcp_init(loop, t);
	uv_tcp_bind(t, uv_ip4_addr(addr, port));
	if (uv_listen((uv_stream_t *)t, 128, on_conn))
		panic("bind %s:%d failed", addr, port);

	lua_pushvalue(L, 3);
	lua_setfield(L, -2, "conn_cb");

	return 1;
}

void luv_tcp_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "tcpsrv", luv_tcpsrv);
}

