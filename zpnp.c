
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

struct srv_s;

typedef struct {
	uv_udp_t s;
	char buf[1440];
} srvdis_t;

typedef struct {
	uv_udp_send_t us;
	uv_buf_t ub;
} clidis_t;

typedef struct {
	struct srv_s *zs;
	uv_udp_t c;
	uv_buf_t ub;
	uv_work_t w;
	int n;
} clinotify_t;

typedef struct {
	uv_tcp_t s;
} srvtrack_t;

typedef struct {
	uv_tcp_t s;
} srvdata_t;

typedef struct {
	struct srv_s *zs;
	parser_t p;
	uv_tcp_t c;
	uv_write_t w;
	uv_buf_t ub;
	char buf[2048];
} clidata_t;

typedef struct {
	struct srv_s *zs;
	struct sockaddr_in sa;
	uv_tcp_t c;
	queue_t q;
	char buf[1];
} clitrack_t;

typedef struct {
	int n, i;
	struct srv_s *zs;
} stopreq_t;

typedef struct srv_s {
	srvdis_t sdis;
	srvdata_t sdata;
	srvtrack_t strack;

	lua_State *L;
	uv_loop_t *loop;

	char *name;
	uint32_t uuid;

	queue_t conns;
} srv_t;

static uv_buf_t srvdis_allocbuf(uv_handle_t *h, size_t len) {
	srv_t *zs = (srv_t *)h->data;
	srvdis_t *sd = &zs->sdis;
	return uv_buf_init(sd->buf, sizeof(sd->buf));
}

static void clidis_on_write(uv_udp_send_t *us, int stat) {
	clidis_t *cd = (clidis_t *)us->data;

	free(cd->ub.base);
	free(cd);
}

static void srvdis_on_read(uv_udp_t *h, ssize_t n, uv_buf_t buf, struct sockaddr *addr, unsigned flags) {
	srv_t *zs = (srv_t *)h->data;
	clidis_t *cd = (clidis_t *)zalloc(sizeof(clidis_t));

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

	cd->ub = uv_buf_init(m.buf, m.len);
	cd->us.data = cd;
	uv_udp_send(&cd->us, h, &cd->ub, 1, *(struct sockaddr_in *)addr, clidis_on_write);
}

static int ip4addr_same(struct sockaddr_in a, struct sockaddr_in b) {
	return a.sin_addr.s_addr == b.sin_addr.s_addr;
}

static int ip4addr_iszero(struct sockaddr_in sa) {
	return sa.sin_addr.s_addr == 0;
}

static void clidata_on_closed(uv_handle_t *h) {
	clidata_t *cd = (clidata_t *)h->data;
	srv_t *zs = cd->zs;

	debug("closed");
	free(cd);
}

static uv_buf_t clidata_allocbuf(uv_handle_t *h, size_t len) {
	clidata_t *cd = (clidata_t *)h->data;
	return uv_buf_init(cd->buf, sizeof(cd->buf));
}

static void clidata_on_write(uv_write_t *w, int stat) {
	clidata_t *cd = (clidata_t *)w->data;

	free(cd->ub.base);
}

static int lua_clidata_ret(lua_State *L) {
	clidata_t *cd = (clidata_t *)lua_touserptr(L, lua_upvalueindex(1));
	srv_t *zs = cd->zs;
	char *ret = (char *)lua_tostring(L, 1);

	if (ret == NULL) {
		uv_close((uv_handle_t *)&cd->c, clidata_on_closed);
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

	cd->ub = uv_buf_init(m.buf, m.len);
	cd->w.data = cd;
	uv_write(&cd->w, (uv_stream_t *)&cd->c, &cd->ub, 1, clidata_on_write);

	return 0;
}

static void clidata_parser_on_end(parser_t *p, msg_t *m) {
	clidata_t *cd = (clidata_t *)p->data;
	srv_t *zs = cd->zs;
	lua_State *L = zs->L;

	debug("type=%x", m->type);

	if (m->type == MT_DATA) {
		lua_getglobal(L, "zpnp_on_action");
		lua_pushstring(L, m->data);
		lua_pushuserptr(L, cd);
		lua_pushcclosure(L, lua_clidata_ret, 1);
		lua_call_or_die(L, 2, 0);
	} else
		uv_close((uv_handle_t *)&cd->c, clidata_on_closed);
}

static void clidata_on_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	clidata_t *cd = (clidata_t *)st->data;

	debug("n=%d", n);

	if (n < 0) 
		uv_close((uv_handle_t *)st, clidata_on_closed);
	else
		parser_parse(&cd->p, buf.base, n);
}

static void srvdata_on_conn(uv_stream_t *st, int stat) {
	srv_t *zs = (srv_t *)st->data;

	clidata_t *cd = (clidata_t *)zalloc(sizeof(clidata_t));
	cd->zs = zs;

	uv_tcp_t *t = &cd->c;
	t->data = cd;
	uv_tcp_init(st->loop, t);

	cd->p.data = cd;
	cd->p.on_end = clidata_parser_on_end;
	parser_init(&cd->p);

	if (uv_accept(st, (uv_stream_t *)t)) {
		warn("accept failed");
		uv_close((uv_handle_t *)t, clidata_on_closed);
		return;
	}
	debug("accepted");

	uv_read_start((uv_stream_t *)t, clidata_allocbuf, clidata_on_read);
}

static void clitrack_del(srv_t *zs, clitrack_t *del) {
	queue_t *q;
	queue_foreach(q, &zs->conns) {
		clitrack_t *ct = queue_data(q, clitrack_t, q);
		if (ct == del)
			queue_remove(q);
	}

	char ip[32]; uv_ip4_name(&del->sa, ip, sizeof(ip));
	info("quit %s", ip);
}

static void clitrack_add(srv_t *zs, clitrack_t *add) {
	queue_t *q;
	queue_foreach(q, &zs->conns) {
		clitrack_t *ct = queue_data(q, clitrack_t, q);
		if (ip4addr_same(ct->sa, add->sa))
			return;
	}
	queue_insert_tail(&zs->conns, &add->q);

	char ip[32]; uv_ip4_name(&add->sa, ip, sizeof(ip));
	info("join %s", ip);
}

static void clitrack_on_closed(uv_handle_t *h) {
	clitrack_t *ct = (clitrack_t *)h->data;
	srv_t *zs = ct->zs;

	clitrack_del(zs, ct);

	debug("closed");
	free(ct);
}

static uv_buf_t clitrack_allocbuf(uv_handle_t *h, size_t len) {
	clitrack_t *ct = (clitrack_t *)h->data;
	return uv_buf_init(ct->buf, sizeof(ct->buf));
}

static void clitrack_on_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	clitrack_t *ct = (clitrack_t *)st->data;

	debug("n=%d", n);

	if (n < 0)
		uv_close((uv_handle_t *)st, clitrack_on_closed);
}

static void srvtrack_on_conn(uv_stream_t *st, int stat) {
	srv_t *zs = (srv_t *)st->data;

	clitrack_t *ct = (clitrack_t *)zalloc(sizeof(clitrack_t));
	ct->zs = zs;

	uv_tcp_t *t = &ct->c;
	t->data = ct;
	uv_tcp_init(st->loop, t);

	if (uv_accept(st, (uv_stream_t *)t)) {
		warn("accept failed");
		uv_close((uv_handle_t *)t, clitrack_on_closed);
		return;
	}

	int len = sizeof(ct->sa);
	uv_tcp_getpeername(t, (struct sockaddr *)&ct->sa, &len);
	clitrack_add(zs, ct);

	uv_read_start((uv_stream_t *)t, clitrack_allocbuf, clitrack_on_read);
}

static void clinotify_broadcast_done(uv_work_t *w, int stat) {
	clinotify_t *cn = (clinotify_t *)w->data;
	free(cn->ub.base);
	free(cn);
}

static void clinotify_broadcast_thread(uv_work_t *w) {
	clinotify_t *cn = (clinotify_t *)w->data;

	debug("n=%d", cn->ub.len);

	int fd = socket(AF_INET, SOCK_DGRAM, 0); 
	if (fd < 0)
		return;

	int v = 1;
	setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &v, sizeof(v));

	struct sockaddr_in si = {}; 
	si.sin_family = AF_INET;
	si.sin_port = htons(PORT_NOTIFY);
	si.sin_addr.s_addr = htonl(INADDR_BROADCAST);

	sendto(fd, cn->ub.base, cn->ub.len, 0, (struct sockaddr *)&si, sizeof(si));
	close(fd);
}

static void clinotify_broadcast(srv_t *zs, void *buf, int len) {
	clinotify_t *cn = (clinotify_t *)zalloc(sizeof(clinotify_t));
	cn->zs = zs;
	cn->ub = uv_buf_init(buf, len);
	cn->w.data = cn;
	debug("n=%d", len);
	uv_queue_work(zs->loop, &cn->w, clinotify_broadcast_thread, clinotify_broadcast_done);
}

static void clinotify_singlecast_on_closed(uv_handle_t *h) {
	clinotify_t *cn = (clinotify_t *)h->data;
	
	debug("closed");

	free(cn->ub.base);
	free(cn);
}

static void clinotify_singlecast_on_write(uv_udp_send_t *us, int stat) {
	clinotify_t *cn = (clinotify_t *)us->data;
	free(us);
	
	debug("done");

	cn->n--;
	if (cn->n == 0)
		uv_close((uv_handle_t *)&cn->c, clinotify_singlecast_on_closed);
}

static void clinotify_singlecast(srv_t *zs, void *buf, int len) {
	clinotify_t *cn = (clinotify_t *)zalloc(sizeof(clinotify_t));

	uv_udp_t *u = &cn->c;
	uv_udp_init(zs->loop, u);
	u->data = cn;
	cn->ub = uv_buf_init(buf, len);

	queue_t *q;
	queue_foreach(q, &zs->conns) {
		clitrack_t *ct = queue_data(q, clitrack_t, q);

		char ip[32]; uv_ip4_name(&ct->sa, ip, sizeof(ip));
		info("> %s len=%d", ip, len);

		uv_udp_send_t *us = (uv_udp_send_t *)zalloc(sizeof(uv_udp_send_t));
		us->data = cn;

		struct sockaddr_in sa = ct->sa;
		sa.sin_port = htons(PORT_NOTIFY);

		uv_udp_send(us, u, &cn->ub, 1, sa, clinotify_singlecast_on_write);
		cn->n++;
	}
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
		clinotify_broadcast(zs, m.buf, m.len);
	} else {
		m.type = MT_DATA;
		m.data = str;
		m.datalen = strlen(str);
		msg_allocfill(&m);
		clinotify_singlecast(zs, m.buf, m.len);
	}

	return 0;
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

static void stopreq_on_closed(uv_handle_t *h) {
	stopreq_t *sq = (stopreq_t *)h->data;

	sq->i++;
	if (sq->i == sq->n)
		info("stopped");
}

static int lua_zpnp_stop(lua_State *L) {
	srv_t *zs = (srv_t *)lua_touserptr(L, lua_upvalueindex(1));

	info("stopping");

	stopreq_t *sq = (stopreq_t *)zalloc(sizeof(stopreq_t));
	sq->zs = zs;

	zs->sdata.s.data = sq;
	uv_close((uv_handle_t *)&zs->sdata.s, stopreq_on_closed);
	sq->n++;

	zs->sdis.s.data = sq;
	uv_close((uv_handle_t *)&zs->sdis.s, stopreq_on_closed);
	sq->n++;

	zs->strack.s.data = sq;
	uv_close((uv_handle_t *)&zs->strack.s, stopreq_on_closed);
	sq->n++;

	queue_t *q;
	queue_foreach(q, &zs->conns) {
		clitrack_t *ct = queue_data(q, clitrack_t, q);
		uv_read_stop((uv_stream_t *)&ct->c);
		ct->c.data = sq;
		uv_close((uv_handle_t *)&ct->c, stopreq_on_closed);
		sq->n++;
	}

	return 0;
}

static int lua_zpnp_start(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	srv_t *zs = (srv_t *)zalloc(sizeof(srv_t));
	zs->L = L;
	zs->loop = loop;

	queue_init(&zs->conns);

	uv_udp_t *u;
	uv_tcp_t *t;

	u = &zs->sdis.s;
	uv_udp_init(loop, u);
	u->data = zs;
	if (uv_udp_bind(u, uv_ip4_addr("0.0.0.0", PORT_DISCOVERY), 0) == -1) 
		panic("bind udp :%d failed", PORT_DISCOVERY);
	uv_udp_recv_start(u, srvdis_allocbuf, srvdis_on_read);

	t = &zs->sdata.s;
	uv_tcp_init(loop, t);
	t->data = zs;
	if (uv_tcp_bind(t, uv_ip4_addr("0.0.0.0", PORT_DATA)) == -1) 
		panic("bind :%d failed", PORT_DATA);
	if (uv_listen((uv_stream_t *)t, 128, srvdata_on_conn))
		panic("listen :%d failed", PORT_DATA);

	t = &zs->strack.s;
	uv_tcp_init(loop, t);
	t->data = zs;
	if (uv_tcp_bind(t, uv_ip4_addr("0.0.0.0", PORT_TRACKALIVE)) == -1) 
		panic("bind :%d failed", PORT_TRACKALIVE);
	if (uv_listen((uv_stream_t *)t, 128, srvtrack_on_conn))
		panic("listen :%d failed", PORT_TRACKALIVE);

	info("starts");

	lua_pushuserptr(L, zs);
	lua_pushcclosure(L, lua_zpnp_setopt, 1);
	lua_setglobal(L, "zpnp_setopt");

	lua_pushuserptr(L, zs);
	lua_pushcclosure(L, lua_zpnp_notify, 1);
	lua_setglobal(L, "zpnp_notify");

	lua_pushuserptr(L, zs);
	lua_pushcclosure(L, lua_zpnp_stop, 1);
	lua_setglobal(L, "zpnp_stop");

	return 0;
}

/*
 * zpnp_start()
 * zpnp_stop()
 * zpnp_on_action(r, done)
 * zpnp_notify(r)
 * zpnp_setopt{uuid=0x1234, name='Muno'}
 */
void luv_zpnp_init(lua_State *L, uv_loop_t *loop) {
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_zpnp_start, 1);
	lua_setglobal(L, "zpnp_start");
}

