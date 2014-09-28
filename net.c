
#include <sys/socket.h>

#include <uv.h>
#include <lua.h>

#include "strbuf.h"
#include "utils.h"

typedef struct {
	uv_udp_t srv, cli;
	char buf[1440];
	lua_State *L;
	uv_loop_t *loop;
} udpsrv_t;

typedef struct {
	uv_udp_t cli;
	uv_udp_send_t sr;
	uv_buf_t buf;
	struct sockaddr_in peer;
	udpsrv_t *us;
} udpcli_t;

typedef struct {
	uv_tcp_t *h;
	lua_State *L;
	uv_loop_t *loop;
} tcpsrv_t;

typedef struct {
	tcpsrv_t *ts;
	strbuf_t *reqsb;
	uv_tcp_t *h;
	uv_write_t wr;
	uv_buf_t buf;
} tcpcli_t;

static void tcpcli_on_handle_closed(uv_handle_t *h) {
	tcpcli_t *tc = (tcpcli_t *)h->data;
	strbuf_t *sb = tc->reqsb;

	if (sb) 
		strbuf_free(sb);
	free(h);
	free(tc);
}

static uv_buf_t tcp_allocbuf(uv_handle_t *h, size_t len) {
	tcpcli_t *tc = (tcpcli_t *)h->data;
	strbuf_t *sb = tc->reqsb;
	strbuf_ensure_empty_length(sb, len);
	return uv_buf_init(sb->buf + sb->length, len);
}

static void tcp_write(uv_write_t *wr, int stat) {
	tcpcli_t *tc = (tcpcli_t *)wr->data;

	uv_close((uv_handle_t *)tc->h, tcpcli_on_handle_closed);
}

static tcpcli_t *lua_totcpcli(lua_State *L, int i) {
	lua_getfield(L, i, "ctx");
	void *tc = lua_touserptr(L, -1);
	lua_pop(L, 1);
	return tc;
}

// arg[1] = { ctx = [userptr tc] }
// arg[2] = '{...}'
static int tcpcli_ret(lua_State *L) {
	tcpcli_t *tc = lua_totcpcli(L, 1);
	char *retstr = (char *)lua_tostring(L, 2);

	tc->buf = uv_buf_init(retstr, strlen(retstr));
	uv_write(&tc->wr, (uv_stream_t *)tc->h, &tc->buf, 1, tcp_write);

	return 0;
}

static void tcp_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	tcpcli_t *tc = (tcpcli_t *)st->data;

	debug("n=%d", n);

	if (n < 0) {
		strbuf_t *sb = tc->reqsb;
		strbuf_append_fmt(sb, 1, "\x00");
		debug("reqsb: %s", sb->buf);

		tcpsrv_t *ts = tc->ts;
		lua_State *L = ts->L;

		lua_getglobalptr(L, "tcp", ts);
		lua_getfield(L, -1, "handler");
		if (lua_isnil(L, -1))
			panic("tcp_server.handler must be set");

		// resp = { 
		//   ctx = [userptr tc] 
		//   ret = [native function]
		// }
		// tcp_server.handler(req, resp)

		lua_pushstring(L, sb->buf);
		
		lua_newtable(L);
		lua_pushuserptr(L, tc);
		lua_setfield(L, -2, "ctx");
		lua_pushcfunction(L, tcpcli_ret);
		lua_setfield(L, -2, "ret");

		lua_call_or_die(L, 2, 0);
		return;
	}

	tc->reqsb->length += n;
}

static void tcp_on_conn(uv_stream_t *st, int stat) {
	tcpsrv_t *ts = (tcpsrv_t *)st->data;

	tcpcli_t *tc = (tcpcli_t *)zalloc(sizeof(tcpcli_t));
	tc->h = (uv_tcp_t *)zalloc(sizeof(uv_tcp_t));
	tc->h->data = tc;
	tc->reqsb = strbuf_new(4096);
	tc->ts = ts;
	uv_tcp_init(st->loop, tc->h);

	if (uv_accept((uv_stream_t *)ts->h, (uv_stream_t *)tc->h)) {
		warn("accept failed");
		uv_close((uv_handle_t *)tc->h, tcpcli_on_handle_closed);
		return;
	}
	info("accepted");

	uv_read_start((uv_stream_t *)tc->h, tcp_allocbuf, tcp_read);
}

// arg[1] = { port = 1111 }
static int lua_tcp_server(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	lua_getfield(L, 1, "port");
	int port = lua_tonumber(L, -1);
	lua_pop(L, 1);

	if (port == 0)
		panic("port must be set");

	tcpsrv_t *ts = (tcpsrv_t *)zalloc(sizeof(tcpsrv_t));
	ts->h = (uv_tcp_t *)zalloc(sizeof(uv_tcp_t));
	ts->h->data = ts;
	ts->L = L;
	ts->loop = loop;

	uv_tcp_init(loop, ts->h);
	uv_tcp_bind(ts->h, uv_ip4_addr("0.0.0.0", port));

	if (uv_listen((uv_stream_t *)ts->h, 128, tcp_on_conn))
		panic("listen :%d failed", port);
	info("tcp listening at :%d", port);
	
	lua_setglobalptr(L, "tcp", ts);

	return 0;
}

static uv_buf_t udp_allocbuf(uv_handle_t *h, size_t len) {
	udpsrv_t *us = (udpsrv_t *)h->data;
	return uv_buf_init(us->buf, sizeof(us->buf)-1);
}

static void udp_write(uv_udp_send_t *sr, int stat) {
	udpcli_t *uc = (udpcli_t *)sr->data;

	free(uc);
}

static udpcli_t *lua_toudpcli(lua_State *L, int i) {
	lua_getfield(L, i, "ctx");
	void *uc = lua_touserptr(L, -1);
	lua_pop(L, 1);
	return uc;
}

// arg[1] = { ctx = [userptr uc] }
// arg[2] = '{...}'
static int lua_udpcli_ret(lua_State *L) {
	udpcli_t *uc = lua_toudpcli(L, 1);
	char *retstr = (char *)lua_tostring(L, 2);

	uc->buf = uv_buf_init(retstr, strlen(retstr));
	uc->sr.data = uc;
	uv_udp_send(&uc->sr, &uc->us->cli, &uc->buf, 1, uc->peer, udp_write);

	return 0;
}

static void udp_read(uv_udp_t *h, ssize_t n, uv_buf_t buf, struct sockaddr *addr, unsigned flags) {
	udpsrv_t *us = (udpsrv_t *)h->data;

	debug("n=%d", n);

	if (n <= 0)
		return;

	us->buf[n] = 0;
	debug("data=%s", us->buf);

	udpcli_t *uc = (udpcli_t *)zalloc(sizeof(udpcli_t));
	uc->peer = *(struct sockaddr_in *)addr;
	uc->us = us;

	lua_State *L = us->L;

	// resp = { 
	//   ctx = [userptr uc] 
	//   ret = [native function]
	// }
	// udp_server.handler(req, resp)

	lua_getglobalptr(L, "udp", us);

	lua_getfield(L, -1, "handler");
	if (lua_isnil(L, -1))
		panic("udp_server.handler must be set");

	lua_pushstring(L, us->buf);
	
	lua_newtable(L);
	lua_pushuserptr(L, uc);
	lua_setfield(L, -2, "ctx");
	lua_pushcfunction(L, lua_udpcli_ret);
	lua_setfield(L, -2, "ret");

	lua_call_or_die(L, 2, 0);
}

// arg[1] = { port = 1111 }
static int lua_udp_server(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	lua_getfield(L, 1, "port");
	int port = lua_tonumber(L, -1);
	lua_pop(L, 1);

	if (port == 0)
		panic("port must be set");

	udpsrv_t *us = (udpsrv_t *)zalloc(sizeof(udpsrv_t));
	us->L = L;
	us->loop = loop;
	us->srv.data = us;

	uv_udp_init(loop, &us->srv);
	uv_udp_init(loop, &us->cli);

	if (uv_udp_bind(&us->srv, uv_ip4_addr("0.0.0.0", port), 0) == -1) 
		panic("bind :%d failed", port);
	
	uv_udp_recv_start(&us->srv, udp_allocbuf, udp_read);
	info("udp listening on :%d", port);

	lua_setglobalptr(L, "udp", us);

	return 0;
}

#define IPLEN 24

static void netinfo_enum(char *ip) {
	uv_interface_address_t *info;
	int n, i;
	char defip[IPLEN];

	uv_interface_addresses(&info, &n);
	for (i = 0; i < n; i++) {
		uv_interface_address_t ia = info[i];

		if (!strcmp(ia.name, "en0") || !strcmp(ia.name, "wlan0")) {
			uv_ip4_name(&ia.address.address4, ip, IPLEN);
		}

		debug("name=%s internal=%d", ia.name, ia.is_internal);
	}
	uv_free_interface_addresses(info, n);
}

static int lua_netinfo_ip(lua_State *L) {
	char ip[IPLEN] = {};

	netinfo_enum(ip);

	lua_pushstring(L, ip);
	return 1;
}

void luv_net_init(lua_State *L, uv_loop_t *loop) {
	lua_pushcfunction(L, lua_netinfo_ip);
	lua_setglobal(L, "netinfo_ip");

	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_udp_server, 1);
	lua_setglobal(L, "udp_server");

	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_tcp_server, 1);
	lua_setglobal(L, "tcp_server");
}

