
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

typedef struct {
	uv_udp_t udp;
	uv_udp_t udpcli;
	uv_udp_t udpbc;
	uv_tcp_t tcp;

	uv_udp_send_t sr;

	char buf[1440];
	uv_buf_t ubuf;

	lua_State *L;
	uv_loop_t *loop;

	char *name;
	uint32_t uuid;
} zpnpsrv_t;

typedef struct zpnpcli_s {
	zpnpsrv_t *zs;

	char buf[2048];
	uv_buf_t ubuf;
	msg_t m;

	parser_t parser;

	uv_tcp_t tcp;
	uv_write_t wr;
	uv_udp_send_t sr;
} zpnpcli_t;

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
		.name = zs->name,
		.uuid = zs->uuid,
		.type = MT_DISCOVERY,
	};
	msg_allocfill(&m);
	debug("n=%d", m.len);

	zs->ubuf = uv_buf_init(m.buf, m.len);
	uv_udp_send(&zs->sr, &zs->udpcli, &zs->ubuf, 1, *(struct sockaddr_in *)addr, udp_write_done);

	free(m.buf);
}

static void tcpcli_on_handle_closed(uv_handle_t *h) {
	zpnpcli_t *zc = (zpnpcli_t *)h->data;

	debug("closed");
	free(zc);
}

static uv_buf_t tcpcli_allocbuf(uv_handle_t *h, size_t len) {
	zpnpcli_t *zc = (zpnpcli_t *)h->data;
	return uv_buf_init(zc->buf, sizeof(zc->buf));
}

static void tcpcli_writedone(uv_write_t *wr, int stat) {
	zpnpcli_t *zc = (zpnpcli_t *)wr->data;

	free(zc->m.buf);
	uv_close((uv_handle_t *)&zc->tcp, tcpcli_on_handle_closed);
}

static int lua_tcpcli_ret(lua_State *L) {
	zpnpcli_t *zc = (zpnpcli_t *)lua_touserptr(L, lua_upvalueindex(1));
	zpnpsrv_t *zs = zc->zs;
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
	zpnpcli_t *zc = (zpnpcli_t *)p->data;
	lua_State *L = zc->zs->L;

	debug("type=%x", m->type);

	if (m->type == MT_DATA) {
		lua_getglobal(L, "zpnp_on_recv");
		lua_pushstring(L, m->data);
		lua_pushuserptr(L, zc);
		lua_pushcclosure(L, lua_tcpcli_ret, 1);
		lua_call_or_die(L, 2, 0);
	} else
		uv_close((uv_handle_t *)&zc->tcp, tcpcli_on_handle_closed);
}

static void tcpcli_readdone(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	zpnpcli_t *zc = (zpnpcli_t *)st->data;

	debug("n=%d", n);

	if (n < 0) {
		uv_close((uv_handle_t *)st, tcpcli_on_handle_closed);
		return;
	}

	parser_parse(&zc->parser, buf.base, n);
}

static void tcpcli_on_conn(uv_stream_t *st, int stat) {
	zpnpsrv_t *zs = (zpnpsrv_t *)st->data;

	zpnpcli_t *zc = (zpnpcli_t *)zalloc(sizeof(zpnpcli_t));
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

	uv_read_start((uv_stream_t *)&zc->tcp, tcpcli_allocbuf, tcpcli_readdone);
}

static int lua_zpnp_setopt(lua_State *L) {
	zpnpsrv_t *zs = (zpnpsrv_t *)lua_touserptr(L, lua_upvalueindex(1));
	
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

static void zpnp_notify_done(uv_udp_send_t *sr, int stat) {
	zpnpcli_t *zc = (zpnpcli_t *)sr->data;

	debug("done");

	free(zc->m.buf);
	free(zc);
}

static int lua_zpnp_notify(lua_State *L) {
	zpnpsrv_t *zs = (zpnpsrv_t *)lua_touserptr(L, lua_upvalueindex(1));
	char *str = (char *)lua_tostring(L, 1);

	zpnpcli_t *zc = (zpnpcli_t *)zalloc(sizeof(zpnpcli_t));
	zc->zs = zs;

	if (str == NULL)
		str = "";

	msg_t m = {
		.name = zs->name,
		.uuid = zs->uuid,
		.type = MT_DATA,
		.data = str,
		.datalen = strlen(str),
	};
	msg_allocfill(&m);
	zc->m = m;
	debug("n=%d -> port %d", m.len, PORT_NOTIFY);

	uv_udp_set_broadcast(&zs->udpbc, 1);

	zc->ubuf = uv_buf_init(m.buf, m.len);
	zc->sr.data = zc;
	struct sockaddr_in addr = uv_ip4_addr("0.0.0.0", PORT_NOTIFY);
	uv_udp_send(&zc->sr, &zs->udpbc, &zc->ubuf, 1, addr, zpnp_notify_done);

	return 0;
}
	
static int lua_zpnp_start(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	zpnpsrv_t *zs = (zpnpsrv_t *)zalloc(sizeof(zpnpsrv_t));
	zs->L = L;
	zs->loop = loop;

	uv_tcp_init(loop, &zs->tcp);
	uv_udp_init(loop, &zs->udp);
	uv_udp_init(loop, &zs->udpcli);
	uv_udp_init(loop, &zs->udpbc);

	zs->udp.data = zs;
	zs->tcp.data = zs;

	if (uv_udp_bind(&zs->udp, uv_ip4_addr("0.0.0.0", PORT_DISCOVERY), 0) == -1) 
		panic("bind udp :%d failed", PORT_DISCOVERY);
	
	if (uv_tcp_bind(&zs->tcp, uv_ip4_addr("0.0.0.0", PORT_DATA)) == -1) 
		panic("bind tcp :%d failed", PORT_DATA);

	if (uv_listen((uv_stream_t *)&zs->tcp, 128, tcpcli_on_conn))
		panic("listen :%d failed", PORT_DATA);
	
	uv_udp_recv_start(&zs->udp, udp_allocbuf, udp_read);

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
 * zpnp_on_recv(r, done)
 * zpnp_notify(r)
 * zpnp_setopt{uuid=0x1234, name='Muno'}
 */
void luv_zpnp_init(lua_State *L, uv_loop_t *loop) {
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_zpnp_start, 1);
	lua_setglobal(L, "zpnp_start");
}

