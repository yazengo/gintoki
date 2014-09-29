
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <lua.h>
#include <uv.h>

#include "utils.h"
#include "strbuf.h"

#include "zpnp_msg.c"

typedef struct parser_s {
	int tstat;
	int pstat;

	uint32_t u32;
	int tlen, tgot;
	strbuf_t *tstr;

	msg_t m;

	void *data;
	void (*on_end)(struct parser_s *, msg_t *);
} parser_t;

enum {
	PARSE_INIT,
	PARSE_VERSION,
	PARSE_TYPE,
	PARSE_UUID,
	PARSE_NAME,
	PARSE_DATA,

	PARSE_END,
};

enum {
	TOKEN_U32,
	TOKEN_BUF,
};

static void parser_init(parser_t *p) {
	p->pstat = PARSE_VERSION;
	p->tstat = TOKEN_U32; p->tgot = 0; p->tlen = 4;
}

static void parser_gothdr(parser_t *p) {
	p->m.name = strdup(p->tstr->buf);
	strbuf_free(p->tstr);

	if (p->m.type == MT_DATA) {
		p->pstat = PARSE_DATA;
		p->tstat = TOKEN_U32; p->tgot = 0; p->tlen = 4;
	} else {
		p->pstat = PARSE_END;
		p->on_end(p, &p->m);
		free(p->m.name);
		strbuf_free(p->tstr);
	}
}

static void parser_gottoken(parser_t *p) {
	debug("pstat=%d tstat=%d u32=0x%x", p->pstat, p->tstat, p->u32);

	switch (p->pstat) {
	case PARSE_VERSION:
		if (p->u32 != MT_VERSION) {
			p->pstat = PARSE_END;
			p->on_end(p, NULL);
		} else {
			p->pstat = PARSE_TYPE;
			p->tstat = TOKEN_U32; p->tgot = 0; p->tlen = 4;
		}
		break;

	case PARSE_TYPE:
		p->m.type = p->u32;
		if (p->m.type != MT_DATA) {
			p->pstat = PARSE_END;
			p->on_end(p, NULL);
		} else {
			p->pstat = PARSE_UUID;
			p->tstat = TOKEN_U32; p->tgot = 0; p->tlen = 4;
		}
		break;

	case PARSE_UUID:
		p->pstat = PARSE_NAME;
		p->tstat = TOKEN_U32; p->tgot = 0; p->tlen = 4;
		break;

	case PARSE_NAME:
		if (p->tstat == TOKEN_U32) {
			p->tstat = TOKEN_BUF; p->tgot = 0; p->tlen = p->u32;
			p->tstr = strbuf_new(256);
		} else if (p->tstat == TOKEN_BUF) {
			parser_gothdr(p);
		} else {
			p->pstat = PARSE_END;
			p->on_end(p, NULL);
		}
		break;

	case PARSE_DATA:
		if (p->tstat == TOKEN_U32) {
			p->tstat = TOKEN_BUF; p->tgot = 0; p->tlen = p->u32;
			p->tstr = strbuf_new(256);
		} else if (p->tstat == TOKEN_BUF) {
			p->pstat = PARSE_END;
			p->m.data = p->tstr->buf;
			p->on_end(p, &p->m);
			free(p->m.name);
			strbuf_free(p->tstr);
		} else {
			p->pstat = PARSE_END;
			p->on_end(p, NULL);
		}
		break;

	default:
		break;
	}
}

static void parser_parse(parser_t *p, void *buf, int len) {
	while (len && p->pstat != PARSE_END) {
		debug("tgot=%d tlen=%d len=%d", p->tgot, p->tlen, len);
		int need = p->tlen - p->tgot;
		if (len < need)
			need = len;
		if (p->tstat == TOKEN_U32)
			memcpy(((void *)&p->u32) + p->tgot, buf, need);
		else 
			strbuf_append_mem(p->tstr, buf, need);
		len -= need;
		buf += need;
		p->tgot += need;
		if (p->tgot == p->tlen) {
			if (p->tstat == TOKEN_BUF)
				strbuf_append_mem(p->tstr, "", 1);
			parser_gottoken(p);
		}
	}
}

struct zpnpcli_s;

#define MAX_PEERS 64

typedef struct {
	uv_udp_t udp;
	uv_udp_t udpcli;
	uv_udp_t udpbc;
	uv_tcp_t tcp;
	uv_tcp_t tcptrack;

	uv_udp_send_t sr;

	char buf[1440];
	uv_buf_t ubuf, ubuf2;

	lua_State *L;
	uv_loop_t *loop;
	uv_work_t work;

	char *name;
	uint32_t uuid;

	struct sockaddr_in peers[MAX_PEERS];
} srv_t;

typedef struct zpnpcli_s {
	srv_t *zs;

	char buf[2048];
	uv_buf_t ubuf;
	msg_t m;

	struct sockaddr_in peer;

	parser_t parser;

	uv_tcp_t tcp;
	uv_write_t wr;
	uv_udp_send_t sr;
} cli_t;

static uv_buf_t udp_allocbuf(uv_handle_t *h, size_t len) {
	srv_t *zs = (srv_t *)h->data;
	return uv_buf_init(zs->buf, sizeof(zs->buf));
}

static void udpsubs_write_done(uv_udp_send_t *sr, int stat) {
	free(sr->data);
}

static void udpsubs_read(uv_udp_t *h, ssize_t n, uv_buf_t buf, struct sockaddr *addr, unsigned flags) {
	srv_t *zs = (srv_t *)h->data;
	lua_State *L = zs->L;

	debug("n=%d", n);

	if (n <= 0)
		return;

	msg_t m = {
		.name = zs->name,
		.uuid = zs->uuid,
		.type = MT_DISCOVERY,
	};
	msg_allocfill(&m);
	debug("n=%d uuid=%x", m.len, m.uuid);

	zs->ubuf = uv_buf_init(m.buf, m.len);
	zs->sr.data = m.buf;
	uv_udp_send(&zs->sr, &zs->udpcli, &zs->ubuf, 1, *(struct sockaddr_in *)addr, udpsubs_write_done);
}

static int ip4addr_same(struct sockaddr_in a, struct sockaddr_in b) {
	return a.sin_addr.s_addr == b.sin_addr.s_addr;
}

static int ip4addr_iszero(struct sockaddr_in sa) {
	return sa.sin_addr.s_addr == 0;
}

static void trackconn_del(srv_t *zs, struct sockaddr_in sa) {
	int i;
	for (i = 0; i < MAX_PEERS; i++) {
		if (ip4addr_same(zs->peers[i], sa)) {
			struct sockaddr_in sa = {};
			zs->peers[i] = sa;
			return;
		}
	}
}

static void trackconn_add(srv_t *zs, struct sockaddr_in sa) {
	int i, empty = -1;
	for (i = 0; i < MAX_PEERS; i++) {
		if (ip4addr_same(zs->peers[i], sa))
			return;
		if (ip4addr_iszero(zs->peers[i]))
			empty = i;
	}
	if (empty != -1)
		zs->peers[empty] = sa;
}

static void tcpcli_on_handle_closed(uv_handle_t *h) {
	cli_t *zc = (cli_t *)h->data;
	srv_t *zs = zc->zs;

	debug("closed");
	free(zc);
}

static uv_buf_t tcpcli_allocbuf(uv_handle_t *h, size_t len) {
	cli_t *zc = (cli_t *)h->data;
	return uv_buf_init(zc->buf, sizeof(zc->buf));
}

static void tcpcli_writedone(uv_write_t *wr, int stat) {
	cli_t *zc = (cli_t *)wr->data;

	free(zc->m.buf);
	uv_close((uv_handle_t *)&zc->tcp, tcpcli_on_handle_closed);
}

static int lua_tcpcli_ret(lua_State *L) {
	cli_t *zc = (cli_t *)lua_touserptr(L, lua_upvalueindex(1));
	srv_t *zs = zc->zs;
	char *ret = (char *)lua_tostring(L, 1);

	if (ret == NULL) {
		uv_close((uv_handle_t *)&zc->tcp, tcpcli_on_handle_closed);
		return 0;
	}

	msg_t m = {
		.name = zs->name,
		.uuid = zs->uuid,
		.type = MT_DATA,
		.data = ret,
		.datalen = strlen(ret),
	};
	msg_allocfill(&m);
	debug("n=%d", m.len);

	zc->m = m;
	zc->ubuf = uv_buf_init(m.buf, m.len);
	zc->wr.data = zc;
	uv_write(&zc->wr, (uv_stream_t *)&zc->tcp, &zc->ubuf, 1, tcpcli_writedone);

	return 0;
}

static void parser_on_end(parser_t *p, msg_t *m) {
	cli_t *zc = (cli_t *)p->data;
	srv_t *zs = zc->zs;
	lua_State *L = zc->zs->L;

	debug("type=%x", m->type);

	if (m->type == MT_DATA) {
		lua_getglobal(L, "zpnp_on_action");
		lua_pushstring(L, m->data);
		lua_pushuserptr(L, zc);
		lua_pushcclosure(L, lua_tcpcli_ret, 1);
		lua_call_or_die(L, 2, 0);
	} else
		uv_close((uv_handle_t *)&zc->tcp, tcpcli_on_handle_closed);
}

static void tcpcli_readdone(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	cli_t *zc = (cli_t *)st->data;
	srv_t *zs = zc->zs;

	debug("n=%d", n);

	if (n < 0) {
		uv_close((uv_handle_t *)st, tcpcli_on_handle_closed);
		return;
	}

	parser_parse(&zc->parser, buf.base, n);
}

static void tcptrack_on_handle_closed(uv_handle_t *h) {
	cli_t *zc = (cli_t *)h->data;
	srv_t *zs = zc->zs;

	trackconn_del(zs, zc->peer);

	debug("closed");
	free(zc);
}

static void tcptrack_readdone(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	cli_t *zc = (cli_t *)st->data;

	debug("n=%d", n);

	if (n < 0) {
		uv_close((uv_handle_t *)st, tcptrack_on_handle_closed);
		return;
	}
}

static void tcptrack_on_conn(uv_stream_t *st, int stat) {
	srv_t *zs = (srv_t *)st->data;

	cli_t *zc = (cli_t *)zalloc(sizeof(cli_t));
	zc->zs = zs;

	zc->tcp.data = zc;
	uv_tcp_init(st->loop, &zc->tcp);

	if (uv_accept(st, (uv_stream_t *)&zc->tcp)) {
		warn("accept failed");
		uv_close((uv_handle_t *)&zc->tcp, tcptrack_on_handle_closed);
		return;
	}
	info("accepted");

	int len = sizeof(zc->peer);
	uv_tcp_getpeername(&zc->tcp, (struct sockaddr *)&zc->peer, &len);
	trackconn_add(zs, zc->peer);

	uv_read_start((uv_stream_t *)&zc->tcp, tcpcli_allocbuf, tcptrack_readdone);
}

static void tcpcli_on_conn(uv_stream_t *st, int stat) {
	srv_t *zs = (srv_t *)st->data;

	cli_t *zc = (cli_t *)zalloc(sizeof(cli_t));
	zc->zs = zs;

	zc->tcp.data = zc;
	uv_tcp_init(st->loop, &zc->tcp);

	zc->parser.data = zc;
	zc->parser.on_end = parser_on_end;
	parser_init(&zc->parser);

	if (uv_accept(st, (uv_stream_t *)&zc->tcp)) {
		warn("accept failed");
		uv_close((uv_handle_t *)&zc->tcp, tcpcli_on_handle_closed);
		return;
	}
	debug("accepted");

	int len = sizeof(zc->peer);
	uv_tcp_getpeername(&zc->tcp, (struct sockaddr *)&zc->peer, &len);
	trackconn_add(zs, zc->peer);

	uv_read_start((uv_stream_t *)&zc->tcp, tcpcli_allocbuf, tcpcli_readdone);
}

static int lua_zpnp_setopt(lua_State *L) {
	srv_t *zs = (srv_t *)lua_touserptr(L, lua_upvalueindex(1));
	
	lua_getfield(L, 1, "uuid");
	if (!lua_isnil(L, -1))
		zs->uuid = lua_tonumber(L, -1);

	lua_getfield(L, 1, "name");
	if (!lua_isnil(L, -1)) {
		if (zs->name)
			free(zs->name);
		zs->name = strdup(lua_tostring(L, -1));
	}

	return 0;
}

typedef struct {
	srv_t *zs;
	uv_buf_t buf;
	uv_work_t w;
} notify_t;

static void notify_broadcast_done(uv_work_t *w, int stat) {
	notify_t *zn = (notify_t *)w->data;
	free(zn);
}

static void notify_broadcast_thread(uv_work_t *w) {
	notify_t *zn = (notify_t *)w->data;

	debug("n=%d", zn->buf.len);

	int fd = socket(AF_INET, SOCK_DGRAM, 0); 
	if (fd < 0)
		return;

	int v = 1;
	setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &v, sizeof(v));

	struct sockaddr_in si = {}; 
	si.sin_family = AF_INET;
	si.sin_port = htons(PORT_NOTIFY);
	si.sin_addr.s_addr = htonl(INADDR_BROADCAST);

	sendto(fd, zn->buf.base, zn->buf.len, 0, (struct sockaddr *)&si, sizeof(si));
	close(fd);
}

static void notify_peers_done(uv_work_t *w, int stat) {
	notify_t *zn = (notify_t *)w->data;
	free(zn);
}

static void notify_peers_thread(uv_work_t *w) {
	notify_t *zn = (notify_t *)w->data;
	srv_t *zs = zn->zs;

	int fd = socket(AF_INET, SOCK_DGRAM, 0); 
	if (fd < 0)
		return;

	debug("n=%d", zn->buf.len);

	int i;
	for (i = 0; i < MAX_PEERS; i++) {
		struct sockaddr_in si = zs->peers[i];
		if (ip4addr_iszero(si))
			continue;
		si.sin_port = htons(PORT_NOTIFY);
		sendto(fd, zn->buf.base, zn->buf.len, 0, (struct sockaddr *)&si, sizeof(si));
	}
	close(fd);
}

static void notify_broadcast(srv_t *zs, void *buf, int len) {
	notify_t *zn = (notify_t *)zalloc(sizeof(notify_t));
	zn->zs = zs;
	zn->buf = uv_buf_init(buf, len);
	zn->w.data = zn;
	info("n=%d", len);
	uv_queue_work(zs->loop, &zn->w, notify_broadcast_thread, notify_broadcast_done);
}

static void notify_peers(srv_t *zs, void *buf, int len) {
	notify_t *zn = (notify_t *)zalloc(sizeof(notify_t));
	zn->zs = zs;
	zn->buf = uv_buf_init(buf, len);
	zn->w.data = zn;
	debug("n=%d", len);
	uv_queue_work(zs->loop, &zn->w, notify_peers_thread, notify_peers_done);
}

static int lua_zpnp_notify(lua_State *L) {
	srv_t *zs = (srv_t *)lua_touserptr(L, lua_upvalueindex(1));
	char *str = (char *)lua_tostring(L, 1);

	msg_t m = {
		.name = zs->name,
		.uuid = zs->uuid,
	};

	if (str == NULL) {
		m.type = MT_DISCOVERY;
		msg_allocfill(&m);
		notify_broadcast(zs, m.buf, m.len);
	} else {
		m.type = MT_DATA;
		m.data = str;
		m.datalen = strlen(str);
		msg_allocfill(&m);
		notify_peers(zs, m.buf, m.len);
	}

	return 0;
}
	
static int lua_zpnp_start(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	srv_t *zs = (srv_t *)zalloc(sizeof(srv_t));
	zs->L = L;
	zs->loop = loop;

	uv_tcp_init(loop, &zs->tcp);
	uv_tcp_init(loop, &zs->tcptrack);
	uv_udp_init(loop, &zs->udp);
	uv_udp_init(loop, &zs->udpcli);

	uv_udp_init(loop, &zs->udpbc);
	uv_udp_set_broadcast(&zs->udpbc, 1);

	zs->udp.data = zs;
	zs->tcp.data = zs;
	zs->tcptrack.data = zs;

	if (uv_udp_bind(&zs->udp, uv_ip4_addr("0.0.0.0", PORT_DISCOVERY), 0) == -1) 
		panic("bind udp :%d failed", PORT_DISCOVERY);
	
	if (uv_tcp_bind(&zs->tcp, uv_ip4_addr("0.0.0.0", PORT_DATA)) == -1) 
		panic("bind tcp :%d failed", PORT_DATA);
	if (uv_listen((uv_stream_t *)&zs->tcp, 128, tcpcli_on_conn))
		panic("listen :%d failed", PORT_DATA);
	
	if (uv_tcp_bind(&zs->tcptrack, uv_ip4_addr("0.0.0.0", PORT_TRACKALIVE)) == -1) 
		panic("bind tcp :%d failed", PORT_TRACKALIVE);
	if (uv_listen((uv_stream_t *)&zs->tcptrack, 128, tcptrack_on_conn))
		panic("listen :%d failed", PORT_TRACKALIVE);
	
	uv_udp_recv_start(&zs->udp, udp_allocbuf, udpsubs_read);

	info("starts");

	lua_pushuserptr(L, zs);
	lua_pushcclosure(L, lua_zpnp_setopt, 1);
	lua_setglobal(L, "zpnp_setopt");

	lua_pushuserptr(L, zs);
	lua_pushcclosure(L, lua_zpnp_notify, 1);
	lua_setglobal(L, "zpnp_notify");

	return 0;
}

/*
 * zpnp_start()
 * zpnp_on_action(r, done)
 * zpnp_notify(r)
 * zpnp_setopt{uuid=0x1234, name='Muno'}
 */
void luv_zpnp_init(lua_State *L, uv_loop_t *loop) {
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_zpnp_start, 1);
	lua_setglobal(L, "zpnp_start");
}

