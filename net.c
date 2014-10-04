
#include <sys/socket.h>
#include <sys/mman.h>
#include <unistd.h>

#include <uv.h>
#include <lua.h>

#include "strbuf.h"
#include "utils.h"
#include "http_parser.h"

#define IPLEN 24

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

struct tcpcli_s;

typedef struct {
	uv_tcp_t *h;
	lua_State *L;
	uv_loop_t *loop;

	uv_buf_t (*allocbuf)(uv_handle_t *h, size_t len);
	void (*read)(uv_stream_t *st, ssize_t n, uv_buf_t buf);

	void (*init)(struct tcpcli_s *tc);
	void (*free)(struct tcpcli_s *tc);
} tcpsrv_t;

typedef struct tcpcli_s {
	tcpsrv_t *ts;
	strbuf_t *reqsb;
	uv_tcp_t *h;
	uv_write_t wr;

	uv_buf_t buf;
	char sbuf[4096];

	void *data;
} tcpcli_t;

typedef struct {
	http_parser *hp;
	http_parser_settings hpconf;

	char *url;
	char *etag;

	char *hkey;
	char *hval;

	strbuf_t *rethdr;
	char *retbody;
	char *mimetype;

	unsigned read_done:1;

	int fd;
	off_t flen;
	void *fdata;
	char *fpath;

	uv_buf_t buf[2];
	uv_work_t w;
} httpcli_t;

static void tcpcli_on_handle_closed(uv_handle_t *h) {
	tcpcli_t *tc = (tcpcli_t *)h->data;
	tcpsrv_t *ts = tc->ts;

	debug("closed");

	ts->free(tc);
	free(h);
	free(tc);
}

static void tcpcli_write(uv_write_t *wr, int stat) {
	tcpcli_t *tc = (tcpcli_t *)wr->data;

	debug("done");

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
	tc->wr.data = tc;
	uv_write(&tc->wr, (uv_stream_t *)tc->h, &tc->buf, 1, tcpcli_write);

	return 0;
}

static void tcpcli_init(tcpcli_t *tc) {
	tc->reqsb = strbuf_new(4096);
}

static void tcpcli_free(tcpcli_t *tc) {
	strbuf_free(tc->reqsb);
}

static uv_buf_t tcpcli_allocbuf(uv_handle_t *h, size_t len) {
	tcpcli_t *tc = (tcpcli_t *)h->data;
	strbuf_t *sb = tc->reqsb;
	strbuf_ensure_empty_length(sb, len);
	return uv_buf_init(sb->buf + sb->length, len);
}

static void tcpcli_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	tcpcli_t *tc = (tcpcli_t *)st->data;

	debug("n=%d", n);

	if (n >= 0) {
		tc->reqsb->length += n;
		return;
	}

	strbuf_t *sb = tc->reqsb;
	strbuf_append_fmt(sb, 1, "\x00");
	debug("reqsb: %s", sb->buf);

	tcpsrv_t *ts = tc->ts;
	lua_State *L = ts->L;

	// resp = { 
	//   ctx = [userptr tc] 
	//   ret = [native function]
	// }
	// tcp_server.handler(req, resp)

	lua_getglobalptr(L, "tcp", ts);
	lua_getfield(L, -1, "handler");
	if (lua_isnil(L, -1))
		panic("tcp_server.handler must be set");

	lua_pushstring(L, sb->buf);

	lua_newtable(L);
	lua_pushuserptr(L, tc);
	lua_setfield(L, -2, "ctx");
	lua_pushcfunction(L, tcpcli_ret);
	lua_setfield(L, -2, "ret");

	lua_call_or_die(L, 2, 0);
}

static void tcp_on_conn(uv_stream_t *st, int stat) {
	tcpsrv_t *ts = (tcpsrv_t *)st->data;

	tcpcli_t *tc = (tcpcli_t *)zalloc(sizeof(tcpcli_t));
	tc->h = (uv_tcp_t *)zalloc(sizeof(uv_tcp_t));
	tc->h->data = tc;
	tc->ts = ts;
	uv_tcp_init(st->loop, tc->h);

	ts->init(tc);

	if (uv_accept((uv_stream_t *)ts->h, (uv_stream_t *)tc->h)) {
		warn("accept failed");
		uv_close((uv_handle_t *)tc->h, tcpcli_on_handle_closed);
		return;
	}
	info("accepted");

	uv_read_start((uv_stream_t *)tc->h, ts->allocbuf, ts->read);
}

static void lua_tcpsrv_init(lua_State *L, tcpsrv_t *ts) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	lua_getfield(L, 1, "port");
	int port = lua_tonumber(L, -1);
	lua_pop(L, 1);

	if (port == 0)
		panic("port must be set");

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
}

// arg[1] = { port = 1111 }
static int lua_tcp_server(lua_State *L) {
	tcpsrv_t *ts = (tcpsrv_t *)zalloc(sizeof(tcpsrv_t));
	lua_tcpsrv_init(L, ts);
	ts->allocbuf = tcpcli_allocbuf;
	ts->read = tcpcli_read;
	ts->init = tcpcli_init;
	ts->free = tcpcli_free;
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

	char ip[IPLEN];
	uv_ip4_name((struct sockaddr_in *)addr, ip, IPLEN);
	debug("peer ip=%s", ip);

	struct sockaddr_in loaddr;
	int salen = sizeof(loaddr);
	uv_udp_getsockname(h, (struct sockaddr *)&loaddr, &salen);
	uv_ip4_name(&loaddr, ip, IPLEN);
	debug("local ip=%s", ip);

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

static uv_buf_t httpcli_allocbuf(uv_handle_t *h, size_t len) {
	tcpcli_t *tc = (tcpcli_t *)h->data;
	return uv_buf_init(tc->sbuf, sizeof(tc->sbuf));
}

static int httpcli_req_url(lua_State *L) {
	tcpcli_t *tc = lua_totcpcli(L, 1);
	httpcli_t *hc = (httpcli_t *)tc->data;

	lua_pushstring(L, hc->url);
	return 1;
}

static int httpcli_req_body(lua_State *L) {
	tcpcli_t *tc = lua_totcpcli(L, 1);
	httpcli_t *hc = (httpcli_t *)tc->data;

	lua_pushstring(L, tc->reqsb->buf);
	return 1;
}

#include "mime.c"

static int mime_cmp(const void *key, const void *_m) {
	mime_t *m = (mime_t *)_m;
	return strcmp(key, m->ext);
}

static char *mime_find(char *ext) {
	mime_t *m = (mime_t *)bsearch(ext, mimes, MIMES_NR, sizeof(mime_t), mime_cmp);
	if (m)
		return m->type;
	return NULL;
}

#define SERVER "Server: Nareix\r\n"

static void httpcli_retfile_close_done(uv_work_t *w, int stat) {
	tcpcli_t *tc = (tcpcli_t *)w->data;
	httpcli_t *hc = (httpcli_t *)tc->data;

	uv_close((uv_handle_t *)tc->h, tcpcli_on_handle_closed);
}

static void httpcli_retfile_close(uv_work_t *w) {
	tcpcli_t *tc = (tcpcli_t *)w->data;
	httpcli_t *hc = (httpcli_t *)tc->data;

	if (hc->fdata)
		munmap(hc->fdata, hc->flen);
	if (hc->fd)
		close(hc->fd);
}

static void httpcli_write_done(uv_write_t *wr, int stat) {
	tcpcli_t *tc = (tcpcli_t *)wr->data;
	httpcli_t *hc = (httpcli_t *)tc->data;

	if (hc->fpath)
		free(hc->fpath);
	
	if (hc->fd) {
		hc->w.data = tc;
		uv_queue_work(tc->ts->loop, &hc->w, httpcli_retfile_close, httpcli_retfile_close_done);
	} else
		uv_close((uv_handle_t *)tc->h, tcpcli_on_handle_closed);
}

static void httpcli_retfile_open_done(uv_work_t *w, int stat) {
	tcpcli_t *tc = (tcpcli_t *)w->data;
	httpcli_t *hc = (httpcli_t *)tc->data;

	if (hc->fdata == NULL) {
		strbuf_append_fmt_retry(hc->rethdr, 
			"HTTP/1.1 404 Not Found\r\n" SERVER
			"Content-Length: 0\r\n"
			"\r\n"
		);
		hc->buf[0] = uv_buf_init(hc->rethdr->buf, hc->rethdr->length);
		tc->wr.data = tc;
		uv_write(&tc->wr, (uv_stream_t *)tc->h, hc->buf, 1, httpcli_write_done);
		return;
	}

	strbuf_append_fmt_retry(hc->rethdr, "HTTP/1.1 200 OK\r\n" SERVER);
	strbuf_append_fmt_retry(hc->rethdr, "Content-Length: %lld\r\n", hc->flen);
	if (hc->mimetype)
		strbuf_append_fmt_retry(hc->rethdr, "Content-Type: %s\r\n", hc->mimetype);
	strbuf_append_fmt_retry(hc->rethdr, "\r\n");

	hc->buf[0] = uv_buf_init(hc->rethdr->buf, hc->rethdr->length);
	hc->buf[1] = uv_buf_init(hc->fdata, hc->flen);
	tc->wr.data = tc;
	uv_write(&tc->wr, (uv_stream_t *)tc->h, hc->buf, 2, httpcli_write_done);
}

static void httpcli_retfile_open(uv_work_t *w) {
	tcpcli_t *tc = (tcpcli_t *)w->data;
	httpcli_t *hc = (httpcli_t *)tc->data;

	hc->fd = open(hc->fpath, O_RDONLY);
	if (hc->fd < 0) {
		warn("open %s failed", hc->fpath);
		hc->fd = 0;
		return;
	}

	struct stat st;
	if (fstat(hc->fd, &st) < 0)
		return;
	
	hc->flen = st.st_size;
	debug("path=%s size=%d", hc->fpath, hc->flen);

	hc->fdata = mmap(NULL, hc->flen, PROT_READ, MAP_SHARED, hc->fd, 0);
	if (hc->fdata == MAP_FAILED) {
		hc->fdata = NULL;
		return;
	}
}

static int httpcli_resp_retfile(lua_State *L) {
	tcpcli_t *tc = lua_totcpcli(L, 1);
	httpcli_t *hc = (httpcli_t *)tc->data;

	char *path = (char *)lua_tostring(L, 2);
	if (path == NULL)
		panic("path must be set");
	path = strdup(path);

	char *dot = strrchr(path, '.');
	if (dot) 
		hc->mimetype = mime_find(dot+1);

	hc->fpath = path;
	hc->rethdr = strbuf_new(128);

	hc->w.data = tc;
	uv_queue_work(tc->ts->loop, &hc->w, httpcli_retfile_open, httpcli_retfile_open_done);

	return 0;
}

static int httpcli_resp_retjson(lua_State *L) {
	tcpcli_t *tc = lua_totcpcli(L, 1);
	httpcli_t *hc = (httpcli_t *)tc->data;

	hc->retbody = (char *)lua_tostring(L, 2);
	if (hc->retbody == NULL)
		hc->retbody = "";
	int bodylen = strlen(hc->retbody);
	hc->retbody = strdup(hc->retbody);

	hc->rethdr = strbuf_new(128);
	strbuf_append_fmt_retry(hc->rethdr, 
		"HTTP/1.1 200 OK\r\n" SERVER
		"Content-Type: text/json\r\n"
		"Content-Length: %d\r\n"
		"\r\n", bodylen
	);

	hc->buf[0] = uv_buf_init(hc->rethdr->buf, hc->rethdr->length);
	hc->buf[1] = uv_buf_init(hc->retbody, bodylen);

	tc->wr.data = tc;
	uv_write(&tc->wr, (uv_stream_t *)tc->h, hc->buf, 2, httpcli_write_done);

	return 0;
}

static void httpcli_read_done(tcpcli_t *tc) {
	httpcli_t *hc = (httpcli_t *)tc->data;

	strbuf_append_mem(tc->reqsb, "", 1);
	debug("body=%s", tc->reqsb->buf);

	tcpsrv_t *ts = tc->ts;
	lua_State *L = ts->L;

	// r = {
	//   ctx = [userptr tc]
	//   body = [native function]
	//   url = [native function]
	//   retfile = [native function]
	//   retjson = [native function]
	// }
	// http_server.handler(r)

	lua_getglobalptr(L, "tcp", ts);
	lua_getfield(L, -1, "handler");
	if (lua_isnil(L, -1))
		panic("http_server.handler must be set");

	lua_newtable(L);
	lua_pushuserptr(L, tc);
	lua_setfield(L, -2, "ctx");
	lua_pushcfunction(L, httpcli_req_url);
	lua_setfield(L, -2, "url");
	lua_pushcfunction(L, httpcli_req_body);
	lua_setfield(L, -2, "body");
	lua_pushcfunction(L, httpcli_resp_retjson);
	lua_setfield(L, -2, "retjson");
	lua_pushcfunction(L, httpcli_resp_retfile);
	lua_setfield(L, -2, "retfile");

	lua_call_or_die(L, 1, 0);
}

static void httpcli_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	tcpcli_t *tc = (tcpcli_t *)st->data;
	httpcli_t *hc = (httpcli_t *)tc->data;

	if (n >= 0) 
		http_parser_execute(hc->hp, &hc->hpconf, buf.base, n);
	else {
		debug("closed early");
		uv_close((uv_handle_t *)tc->h, tcpcli_on_handle_closed);
	}
}

static int httpcli_on_url(http_parser *hp, const char *at, size_t length) {
	tcpcli_t *tc = (tcpcli_t *)hp->data;
	httpcli_t *hc = (httpcli_t *)tc->data;

	hc->url = strndup(at, length);
	debug("url=%s", hc->url);

	return 0;
}

static int httpcli_on_body(http_parser *hp, const char *at, size_t length) {
	tcpcli_t *tc = (tcpcli_t *)hp->data;
	httpcli_t *hc = (httpcli_t *)tc->data;

	strbuf_append_mem(tc->reqsb, at, length);

	return 0;
}

static int httpcli_on_header_field(http_parser *hp, const char *at, size_t length) {
	tcpcli_t *tc = (tcpcli_t *)hp->data;
	httpcli_t *hc = (httpcli_t *)tc->data;

	hc->hkey = strndup(at, length);
	return 0;
}

static int httpcli_on_header_value(http_parser *hp, const char *at, size_t length) {
	tcpcli_t *tc = (tcpcli_t *)hp->data;
	httpcli_t *hc = (httpcli_t *)tc->data;

	hc->hval = strndup(at, length);

	debug("%s: %s", hc->hkey, hc->hval);
	free(hc->hkey);
	free(hc->hval);

	return 0;
}

static int httpcli_on_message_complete(http_parser *hp) {
	tcpcli_t *tc = (tcpcli_t *)hp->data;
	httpcli_t *hc = (httpcli_t *)tc->data;

	debug("complete");

	uv_read_stop((uv_stream_t *)tc->h);
	httpcli_read_done(tc);

	return 0;
}

static void httpcli_init(tcpcli_t *tc) {
	tc->reqsb = strbuf_new(4096);

	httpcli_t *hc = (httpcli_t *)zalloc(sizeof(httpcli_t));
	tc->data = hc;

	hc->hp = (http_parser *)zalloc(sizeof(http_parser));
	hc->hp->data = tc;
	http_parser_init(hc->hp, HTTP_REQUEST);

	hc->hpconf.on_url = httpcli_on_url;
	hc->hpconf.on_body = httpcli_on_body;
	hc->hpconf.on_message_complete = httpcli_on_message_complete;
	hc->hpconf.on_header_field = httpcli_on_header_field;
	hc->hpconf.on_header_value = httpcli_on_header_value;
}

static void httpcli_free(tcpcli_t *tc) {
	httpcli_t *hc = (httpcli_t *)tc->data;

	if (hc->url)
		free(hc->url);
	if (hc->rethdr)
		strbuf_free(hc->rethdr);
	if (hc->retbody)
		free(hc->retbody);

	strbuf_free(tc->reqsb);
	free(hc->hp);
	free(hc);
}

static int lua_http_server(lua_State *L) {
	tcpsrv_t *ts = (tcpsrv_t *)zalloc(sizeof(tcpsrv_t));
	lua_tcpsrv_init(L, ts);
	ts->allocbuf = httpcli_allocbuf;
	ts->read = httpcli_read;
	ts->init = httpcli_init;
	ts->free = httpcli_free;
	return 0;
}

#undef SERVER

void luv_net_init(lua_State *L, uv_loop_t *loop) {
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_udp_server, 1);
	lua_setglobal(L, "udp_server");

	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_tcp_server, 1);
	lua_setglobal(L, "tcp_server");

	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_http_server, 1);
	lua_setglobal(L, "http_server");
}

