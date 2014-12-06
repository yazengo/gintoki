#include <string.h>
#include <stdlib.h>
#include "http_parser.h"
#include "luv.h"
#include "pipe.h"
#include "strbuf.h"

typedef struct {
    http_parser *hp;
    http_parser_settings hpconf;

    char *url;
    char *hkey;
    char *hval;
    char *hkey_match;
    char *hval_match;

    pipe_t *pi;
    pipe_t *po;
} httpsrv_t;

static void http_gc(uv_loop_t *loop, void *_l) {
    info("gc");
    pipe_t *p = (pipe_t *)_l;
    httpsrv_t *h = (httpsrv_t *)p->data;
    free(h->hp);
    if (h->url)
        free(h->url);
    if (h->hkey)
        free(h->hkey);
    if (h->hval)
        free(h->hval);
    if (h->hkey_match)
        free(h->hkey_match);
    if (h->hval_match)
        free(h->hval_match);
}

static void close_all(httpsrv_t *h) {
    info("close all");
    pipe_close_read(h->pi);
    pipe_close_write(h->po);
}

static void http_read_done(pipe_t *p, pipebuf_t *pb);

static void http_write_done(struct pipe_s *p, int stat) {
    httpsrv_t *h = (httpsrv_t *)p->data;

    if (stat < 0) {
        close_all(h);
        return;
    }

    pipe_read(h->pi, http_read_done);
}

static void http_read_done(pipe_t *p, pipebuf_t *pb) {
    httpsrv_t *h = (httpsrv_t *)p->data;

    if (pb == NULL) {
        close_all(h);
        return;
    }

    http_parser_execute(h->hp, &h->hpconf, pb->base, pb->len);
    pipe_write(h->po, pb, http_write_done);
}

static int http_on_body(http_parser *hp, const char *at, size_t length) {
    httpsrv_t *h = (httpsrv_t *)hp->data;
    return 0;
}

static int http_on_url(http_parser *hp, const char *at, size_t length) {
    httpsrv_t *h = (httpsrv_t *)hp->data;

    h->url = strndup(at, length);
    debug("url=%s", h->url);

    return 0;
}

static int http_on_message_complete(http_parser *hp) {
    httpsrv_t *h = (httpsrv_t *)hp->data;

    info("complete");

    luv_callfield(h, "on_complete", 0, 0);
    return 0;
}

static int http_on_header_field(http_parser *hp, const char *at, size_t length) {
    httpsrv_t *h = (httpsrv_t *)hp->data;
    h->hkey = strndup(at, length);
    return 0;
}

static int http_on_header_value(http_parser *hp, const char *at, size_t length) {
    httpsrv_t *h = (httpsrv_t *)hp->data;

    h->hval = strndup(at, length);
    debug("%s: %s", h->hkey, h->hval);

	if (!strcmp(h->hkey, h->hkey_match)) {
        h->hval_match = strndup(at, length);
    }

    return 0;
}

static int luv_http_server(lua_State *L, uv_loop_t *loop) {
    httpsrv_t *h = (httpsrv_t *)luv_newctx(L, loop, sizeof(httpsrv_t));

    lua_pushvalue(L, 1);
    lua_setfield(L, -2, "on_complete");

    h->hp = (http_parser *)zalloc(sizeof(http_parser));
    h->hp->data = h;
    luv_setgc(h, http_gc);
    http_parser_init(h->hp, HTTP_REQUEST);

    h->hpconf.on_url = http_on_url;
    h->hpconf.on_body = http_on_body;
    h->hpconf.on_message_complete = http_on_message_complete;
    h->hpconf.on_header_field = http_on_header_field;
    h->hpconf.on_header_value = http_on_header_value;

    h->pi = pipe_new(L, loop);
    h->pi->type = PDIRECT_SINK;
    h->pi->data = h;

    h->po = pipe_new(L, loop);
    h->po->type = PDIRECT_SRC;
    h->po->data = h;

    pipe_read(h->pi, http_read_done);

    return 3;
}

static int luv_http_setopt(lua_State *L, uv_loop_t *loop) {
    httpsrv_t *h = (httpsrv_t *)luv_toctx(L, 1);
	char *op = (char *)lua_tostring(L, 2);

	if (!strcmp(op, "geturl")) {
		lua_pushstring(L, h->url);
		return 1;
	}

	if (!strcmp(op, "getmethod")) {
		lua_pushnumber(L, h->hp->method);
		return 1;
	}

	if (!strcmp(op, "setheader")) {
		h->hkey_match = (char *)lua_tostring(L, 3);
		return 0;
	}

	if (!strcmp(op, "getheader")) {
		lua_pushstring(L, h->hval_match);
		return 1;
	}

	return 0;
}

void luv_http_init(lua_State *L, uv_loop_t *loop) {
    luv_register(L, loop, "http_server", luv_http_server);
	luv_register(L, loop, "http_setopt", luv_http_setopt);
}

