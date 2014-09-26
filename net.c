
#include <sys/socket.h>

#include <uv.h>
#include <lua.h>

#include "strbuf.h"
#include "utils.h"

typedef struct {
	uv_udp_t srv, cli;
	char buf[1440];
} udpsrv_t;

typedef struct {
	uv_udp_t cli;
	uv_udp_send_t *sr;
	uv_buf_t buf;
	struct sockaddr_in peer;
} udpcli_t;

typedef struct {
	uv_tcp_t *h;
} tcpsrv_t;

typedef struct {
	strbuf_t *reqsb;
	int reqgot;
	uv_tcp_t *h;

	char *retstr;
	int retsent;
} tcpcli_t;

static void on_handle_closed(uv_handle_t *h) {
	tcpcli_t *tc = (tcpcli_t *)h->data;
	strbuf_t *sb = tc->reqsb;

	if (sb) {
		strbuf_free(sb);
	}

	free(h);
}

static uv_buf_t tcp_allocbuf(uv_handle_t *h, size_t len) {
	tcpcli_t *tc = (tcpcli_t *)h->data;
	strbuf_t *sb = tc->reqsb;
	strbuf_ensure_empty_length(sb, len);
	return uv_buf_init(sb->buf + sb->length, len);
}

static void tcp_write(uv_write_t *wr, int stat) {
	tcpcli_t *tc = (tcpcli_t *)wr->data;

	uv_close((uv_handle_t *)tc->h, on_handle_closed);
}

static void tcp_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	tcpcli_t *tc = (tcpcli_t *)st->data;

	debug("n=%d", n);

	if (n < 0) {
		tc->retstr = "hello world";

		uv_write_t *wr = (uv_write_t *)zalloc(sizeof(uv_write_t));
		wr->data = tc;

		strbuf_t *sb = tc->reqsb;
		strbuf_append_fmt(sb, 1, "\x00");
		info("reqsb: %s", sb->buf);

		wr->bufs = (uv_buf_t *)zalloc(sizeof(uv_buf_t));
		wr->bufs[0] = uv_buf_init(tc->retstr, strlen(tc->retstr));
		uv_write(wr, (uv_stream_t *)tc->h, wr->bufs, 1, tcp_write);
		return;
	}

	tc->reqsb->length += n;
}

static void tcp_on_conn(uv_stream_t *st, int stat) {
	tcpsrv_t *ts = (tcpsrv_t *)st->data;

	tcpcli_t *tc = (tcpcli_t *)zalloc(sizeof(tcpcli_t));
	tc->h = (uv_tcp_t *)zalloc(sizeof(uv_tcp_t));
	tc->h->data = tc;
	uv_tcp_init(st->loop, tc->h);
	
	tc->reqsb = strbuf_new(4096);

	if (uv_accept((uv_stream_t *)ts->h, (uv_stream_t *)tc->h)) {
		warn("accept failed");
		uv_close((uv_handle_t *)tc->h, on_handle_closed);
		return;
	}
	info("accepted");

	uv_read_start((uv_stream_t *)tc->h, tcp_allocbuf, tcp_read);
}

static void tcp_new(uv_loop_t *loop) {
	tcpsrv_t *ts = (tcpsrv_t *)zalloc(sizeof(tcpsrv_t));

	ts->h = (uv_tcp_t *)zalloc(sizeof(uv_tcp_t));
	ts->h->data = ts;

	int port = 10240;

	uv_tcp_init(loop, ts->h);
	uv_tcp_bind(ts->h, uv_ip4_addr("0.0.0.0", port));

	if (uv_listen((uv_stream_t *)ts->h, 128, tcp_on_conn)) {
		panic("listen failed");
	}
	info("listening at :%d", port);
}

static uv_buf_t udp_allocbuf(uv_handle_t *h, size_t len) {
	udpsrv_t *us = (udpsrv_t *)h->data;
	return uv_buf_init(us->buf, sizeof(us->buf));
}

static void udp_write(uv_udp_send_t *sr, int stat) {
	udpcli_t *uc = (udpcli_t *)sr->data;

	info("sent");

	free(uc->sr);
	free(uc);
}

static void udp_read(uv_udp_t *h, ssize_t n, uv_buf_t buf, struct sockaddr *addr, unsigned flags) {
	udpsrv_t *us = (udpsrv_t *)h->data;

	debug("n=%d", n);

	if (n <= 0)
		return;

	us->buf[n] = 0;
	info("data=%s", us->buf);

	udpcli_t *uc = (udpcli_t *)zalloc(sizeof(udpcli_t));

	uc->peer = *(struct sockaddr_in *)addr;

	char *str = "hello world";
	uc->buf = uv_buf_init(str, strlen(str));

	uc->sr = (uv_udp_send_t *)zalloc(sizeof(uv_udp_send_t));
	uc->sr->data = uc;
	uv_udp_send(uc->sr, &us->cli, &uc->buf, 1, uc->peer, udp_write);
}

static void udp_new(uv_loop_t *loop) {
	udpsrv_t *us = (udpsrv_t *)zalloc(sizeof(udpsrv_t));
	us->srv.data = us;

	int port = 6888;

	uv_udp_init(loop, &us->srv);
	uv_udp_init(loop, &us->cli);
	uv_udp_bind(&us->srv, uv_ip4_addr("0.0.0.0", port), 0);
	uv_udp_recv_start(&us->srv, udp_allocbuf, udp_read);

	info("listening on :%d", port);
}

static void enum_interfaces() {
	uv_interface_address_t *info;
	int n, i;

	uv_interface_addresses(&info, &n);
	for (i = 0; i < n; i++) {
		uv_interface_address_t ia = info[i];
		char buf[512];

		uv_ip4_name(&ia.address.address4, buf, sizeof(buf));

		debug("name=%s addr=%s internal=%d", ia.name, buf, ia.is_internal);
	}

	uv_free_interface_addresses(info, n);
}

void luv_net_init(lua_State *L, uv_loop_t *loop) {
	enum_interfaces();
	udp_new(loop);
}

