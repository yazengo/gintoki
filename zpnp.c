
#include <lua.h>
#include <uv.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils.h"

#include "zpnp_msg.c"

typedef struct {
	uv_udp_t udp;
	uv_tcp_t tcp;
	char buf[1440];
	lua_State *L;
	uv_loop_t *loop;

	uv_udp_send_t sr;
	uv_buf_t ubuf;
	uv_udp_t udpcli;
} zpnpsrv_t;

typedef struct {
	uv_tcp_t tcp;
} zpnpcli_t;

static zpnpsrv_t *zpnp;

#define UDP_PORT 33123
#define TCP_PORT 33124

static uv_buf_t udp_allocbuf(uv_handle_t *h, size_t len) {
	zpnpsrv_t *zs = (zpnpsrv_t *)h->data;
	return uv_buf_init(zs->buf, sizeof(zs->buf));
}

static void udp_write_done(uv_udp_send_t *sr, int stat) {
	debug("sent");
}

static void udp_read(uv_udp_t *h, ssize_t n, uv_buf_t buf, struct sockaddr *addr, unsigned flags) {
	zpnpsrv_t *zs = (zpnpsrv_t *)h->data;

	debug("n=%d", n);

	if (n <= 0)
		return;

	msg_t m = {
		.name = "Muno",
		.uuid = 0x1234,
		.type = MT_DISCOVERY,
	};
	msg_allocfill(&m);
	debug("resp n=%d", m.len);

	zs->ubuf = uv_buf_init(m.buf, m.len);
	uv_udp_send(&zs->sr, &zs->udpcli, &zs->ubuf, 1, *(struct sockaddr_in *)addr, udp_write_done);
}

static void tcpcli_on_handle_closed(uv_handle_t *h) {
	zpnpcli_t *zc = (zpnpcli_t *)h->data;

	debug("closed");
	free(zc);
}

static void tcp_on_conn(uv_stream_t *st, int stat) {
	zpnpsrv_t *zs = (zpnpsrv_t *)st->data;

	zpnpcli_t *zc = (zpnpcli_t *)zalloc(sizeof(zpnpcli_t));
	zc->tcp.data = zc;
	uv_tcp_init(st->loop, &zc->tcp);

	if (uv_accept(st, (uv_stream_t *)&zc->tcp)) {
		warn("accept failed");
		uv_close((uv_handle_t *)&zc->tcp, tcpcli_on_handle_closed);
		return;
	}
	debug("accepted");
}

static int lua_zpnp_start(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	zpnpsrv_t *zs = (zpnpsrv_t *)zalloc(sizeof(zpnpsrv_t));
	zs->L = L;
	zs->loop = loop;

	uv_udp_init(loop, &zs->udp);
	uv_udp_init(loop, &zs->udpcli);
	uv_tcp_init(loop, &zs->tcp);

	zs->udp.data = zs;
	zs->tcp.data = zs;

	if (uv_udp_bind(&zs->udp, uv_ip4_addr("0.0.0.0", UDP_PORT), 0) == -1) 
		panic("bind udp :%d failed", UDP_PORT);
	
	if (uv_tcp_bind(&zs->tcp, uv_ip4_addr("0.0.0.0", TCP_PORT)) == -1) 
		panic("bind tcp :%d failed", TCP_PORT);

	if (uv_listen((uv_stream_t *)&zs->tcp, 128, tcp_on_conn))
		panic("listen :%d failed", TCP_PORT);
	
	uv_udp_recv_start(&zs->udp, udp_allocbuf, udp_read);

	lua_setglobalptr(L, "zpnp", zs);
	info("starts");

	return 0;
}

/*
 * zpnp_start(uuid)
 * zpnp_on_recv(r, done)
 * zpnp_notify(r)
 */
void luv_zpnp_init(lua_State *L, uv_loop_t *loop) {
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_zpnp_start, 1);
	lua_setglobal(L, "zpnp_start");
}

