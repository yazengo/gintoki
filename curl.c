
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <curl/curl.h>
#include <uv.h>

#include "utils.h"
#include "strbuf.h"

//
// c = curl {
// 	 url = 'xx',
// 	 retfile = 'x',
// 	 reqstr = '',
// 	 done = function (content, st) 
// 	 end,
// 	 connected = function ()
// 	 end,
// }
//
// c.stat() == {
// 	 stat = 'downloading/done/cancelled',
// 	 code = 200, 
// 	 progress = 33.33,
// 	 rx = 12312,
// 	 time = 33.33,
// 	 speed = 1232
// 	 size = 1232.33,
// }
// c.cancel()
//

typedef struct {
	CURL *c;
	struct curl_slist *headers;

	char *proxy;

	int stat;
	const char *err;

	char *reqstr;
	int reqstr_len;

	char *retfname;
	FILE *retfp;
	strbuf_t *retsb;

	int curl_ret;

	long code;
	double size;
	double progress;
	double rx;
	double time;
	double speed;

	int retry;

	uv_loop_t *loop;
	lua_State *L;
} curl_t;

enum {
	DOWNLOADING,
	CANCELLED,
	DONE,
	ERROR,
};

static void getinfo(curl_t *lc) {
	curl_easy_getinfo(lc->c, CURLINFO_SIZE_DOWNLOAD, &lc->rx);
	curl_easy_getinfo(lc->c, CURLINFO_SPEED_DOWNLOAD, &lc->speed);
	curl_easy_getinfo(lc->c, CURLINFO_RESPONSE_CODE, &lc->code);
	curl_easy_getinfo(lc->c, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &lc->size);
	if (lc->size < 0)
		lc->size = 0;
	if (lc->stat == DONE)
		lc->progress = 1.0;
	else if (lc->size > 0)
		lc->progress = lc->rx / lc->size;

	if (lc->stat != CANCELLED && lc->curl_ret) {
		lc->stat = ERROR;
		lc->err = "libcurl error";
	}
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *data) {
	curl_t *lc = (curl_t *)data;

	if (lc->stat == CANCELLED)
		return 0;

	size *= nmemb;

	if (lc->retfp) 
		size = fwrite(ptr, 1, size, lc->retfp);
	if (lc->retsb)
		strbuf_append_mem(lc->retsb, ptr, size);

	lc->rx += size;

	debug("write=%d", size);

	return size;
}

static size_t read_data(void *ptr, size_t size, size_t nmemb, void *data) {
	curl_t *lc = (curl_t *)data;

	if (lc->stat == CANCELLED)
		return 0;

	size *= nmemb;
	if (size > lc->reqstr_len)
		size = lc->reqstr_len;

	memcpy(ptr, lc->reqstr, size);
	lc->reqstr_len -= size;
	lc->reqstr += size;

	debug("read=%d", size);

	return size;
}

static size_t on_header(char *p, size_t size, size_t n, void *data) {
	curl_t *lc = (curl_t *)data;
	lua_State *L = lc->L;

	size *= n;
	int i, dot = -1;

	for (i = 0; i < size; i++)  {
		if (p[i] == ':') {
			dot = i;
			break;
		}
	}

	if (dot != -1 && dot+2 < size-1) {
		lua_getglobalptr(L, "curl", lc);
		lua_getfield(L, -1, "on_header");
		lua_remove(L, -2);
		lua_pushlstring(L, p, dot);
		lua_pushlstring(L, p+dot+2, size-dot-3);
		lua_call_or_die(L, 2, 0);
	}

	return size;
}

static void push_curl_stat(curl_t *lc) {
	lua_State *L = lc->L;

	char *stat = "?";
	switch (lc->stat) {
	case DOWNLOADING: stat = "downloading"; break;
	case DONE: stat = "done"; break;
	case CANCELLED: stat = "cancelled"; break;
	case ERROR: stat = "err"; break;
	}

	lua_newtable(L);

	lua_pushstring(L, stat);
	lua_setfield(L, -2, "stat");
	lua_pushnumber(L, lc->code);
	lua_setfield(L, -2, "code");
	lua_pushnumber(L, lc->progress);
	lua_setfield(L, -2, "progress");
	lua_pushnumber(L, lc->rx);
	lua_setfield(L, -2, "rx");
	lua_pushnumber(L, lc->time);
	lua_setfield(L, -2, "time");
	lua_pushnumber(L, lc->speed);
	lua_setfield(L, -2, "speed");
	lua_pushnumber(L, lc->size);
	lua_setfield(L, -2, "size");
	lua_pushnumber(L, lc->curl_ret);
	lua_setfield(L, -2, "curl_ret");

	if (lc->stat == ERROR) {
		lua_pushstring(L, lc->err);
		lua_setfield(L, -2, "err");
	}

	if (lc->retfname) {
		lua_pushstring(L, lc->retfname);
		lua_setfield(L, -2, "retfname");
	}
}

static curl_t *lua_getcurl(lua_State *L) {
	lua_getfield(L, 1, "ctx");
	return lua_touserptr(L, -1);
}

static int curl_stat(lua_State *L) {
	curl_t *lc = lua_getcurl(L);

	if (lc->stat == DOWNLOADING)
		getinfo(lc);
	push_curl_stat(lc);

	return 1;
}

static int curl_cancel(lua_State *L) {
	curl_t *lc = lua_getcurl(L);

	lc->stat = CANCELLED;

	return 0;
}

static void curl_thread_done(uv_work_t *w, int _) {
	curl_t *lc = (curl_t *)w->data;
	lua_State *L = lc->L;

	free(w);

	if (lc->stat == DOWNLOADING)
		lc->stat = DONE;

	lua_getglobalptr(L, "curl", lc);
	lua_getfield(L, -1, "done");
	lua_remove(L, -2);

	// 1
	if (lc->curl_ret == 0 && lc->retsb) {
		strbuf_append_char(lc->retsb, 0);
		lua_pushstring(L, lc->retsb->buf);
	} else 
		lua_pushnil(L);

	if (lc->retfp)
		fclose(lc->retfp);

	// 2
	getinfo(lc);
	push_curl_stat(lc);

	lua_call_or_die(L, 2, 0);

	curl_easy_cleanup(lc->c);
	if (lc->retsb)
		strbuf_free(lc->retsb);
	if (lc->retfname)
		free(lc->retfname);

	lua_pushnil(L);
	lua_setglobalptr(L, "curl", lc);

	free(lc);
}

static void curl_perform(curl_t *lc) {
	if (lc->retry) {
		for (;;) {
			if (lc->stat == CANCELLED)
				return;
			lc->curl_ret = curl_easy_perform(lc->c);
			if (lc->curl_ret == 0)
				return;
			usleep(lc->retry*1000);
		}
	} else {
		lc->curl_ret = curl_easy_perform(lc->c);
	}
}

static void curl_thread(uv_work_t *w) {
	curl_t *lc = (curl_t *)w->data;

	if (lc->stat != ERROR)
		curl_perform(lc);

	if (lc->headers)
		curl_slist_free_all(lc->headers);

	if (lc->proxy)
		free(lc->proxy);

	debug("ends r=%d", lc->curl_ret);
}

static void curl_setproxy(curl_t *lc, char *proxy) {
	if (proxy == NULL)
		return;

	proxy = strdup(proxy);
	lc->proxy = proxy;

	debug("proxy=%s", proxy);

	char *s = proxy + strlen(proxy) - 1;
	int found = 0;
	while (s > proxy) {
		if (*s == ':') {
			found++;
			break;
		}
		if (!(*s >= '0' && *s <= '9'))
			break;
		s--;
	}

	if (!found) {
		debug("proxy.url=%s", proxy);
		curl_easy_setopt(lc->c, CURLOPT_PROXY, proxy);
		return;
	}
	*s = 0;

	debug("proxy.url=%s", proxy);
	curl_easy_setopt(lc->c, CURLOPT_PROXY, proxy);

	int port = 0;
	sscanf(s+1, "%d", &port);
	if (!port)
		return;

	curl_easy_setopt(lc->c, CURLOPT_PROXYPORT, (long)port);
	debug("proxy.port=%d", port);
}

static void curl_addheader(curl_t *lc, char *name, char *val) {
	if (val == NULL)
		return;

	int namelen = strlen(name);
	int vallen = strlen(val);
	char *s = (char *)zalloc(namelen + vallen + 3);
	memcpy(s, name, namelen);
	memcpy(s + namelen, ": ", 2);
	memcpy(s + namelen + 2, val, vallen);

	lc->headers = curl_slist_append(lc->headers, s);

	free(s);
}

static int curl(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	curl_t *lc = (curl_t *)zalloc(sizeof(curl_t));
	memset(lc, 0, sizeof(curl_t));
	int lc_idx = 2;

	lc->loop = loop;
	lc->L = L;

	lua_getfield(L, 1, "url");
	char *url = (char *)lua_tostring(L, -1);
	if (url == NULL) 
		panic("url must be set");

	lua_getfield(L, 1, "retry");
	lc->retry = lua_tonumber(L, -1);

	lua_getfield(L, 1, "retfile");
	char *retfname = (char *)lua_tostring(L, -1);
	if (retfname)
		lc->retfname = strdup(retfname);
	else
		lc->retsb = strbuf_new(2048);

	if (lc->retfname) {
		lc->retfp = fopen(lc->retfname, "wb+");
		info("'%s' opened", lc->retfname);
		if (lc->retfp == NULL) {
			warn("open '%s' failed", lc->retfname);
			lc->stat = ERROR;
			lc->err = "open_file_failed";
		}
	}

	lc->c = curl_easy_init();

	curl_easy_setopt(lc->c, CURLOPT_URL, url);
	curl_easy_setopt(lc->c, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(lc->c, CURLOPT_WRITEDATA, lc);

	curl_easy_setopt(lc->c, CURLOPT_SSL_VERIFYPEER, 0);

	//curl_easy_setopt(lc->c, CURLOPT_VERBOSE, 1);

	lua_getfield(L, 1, "reqstr");
	lc->reqstr = (char *)lua_tostring(L, -1);
	if (lc->reqstr) {
		//lc->reqstr_len = strlen(lc->reqstr);
		//curl_easy_setopt(lc->c, CURLOPT_READFUNCTION, read_data);
		//curl_easy_setopt(lc->c, CURLOPT_READDATA, lc);
		curl_easy_setopt(lc->c, CURLOPT_POSTFIELDS, lc->reqstr);
	}

	lua_getfield(L, 1, "on_header");
	if (!lua_isnil(L, -1)) {
		curl_easy_setopt(lc->c, CURLOPT_HEADERFUNCTION, on_header);
		curl_easy_setopt(lc->c, CURLOPT_HEADERDATA, lc);
	}

	lua_getfield(L, 1, "proxy");
	char *proxy = (char *)lua_tostring(L, -1);
	curl_setproxy(lc, proxy);

	lua_getfield(L, 1, "content_type");
	curl_addheader(lc, "Content-Type", (char *)lua_tostring(L, -1));

	lua_getfield(L, 1, "user_agent");
	curl_addheader(lc, "User-Agent", (char *)lua_tostring(L, -1));

	lua_getfield(L, 1, "headers");
	int t = lua_gettop(L);
	if (!lua_isnil(L, t)) {
		lua_pushnil(L);
		while (lua_next(L, t)) {
			char *key = (char *)lua_tostring(L, -2);
			char *val = (char *)lua_tostring(L, -1);
			if (key && val)
				curl_addheader(lc, key, val);
			lua_pop(L, 1);
		}
	}

	if (lc->headers)
		curl_easy_setopt(lc->c, CURLOPT_HTTPHEADER, lc->headers);

	// return {
	//   ctx = [userptr lc],
	//   cancel = [native function],
	//   done = done,
	//   stat = [native function],
	// }
	lua_newtable(L);

	lua_pushuserptr(L, lc);
	lua_setfield(L, -2, "ctx");

	lua_pushcfunction(L, curl_cancel);
	lua_setfield(L, -2, "cancel");

	lua_pushcfunction(L, curl_stat);
	lua_setfield(L, -2, "stat");

	lua_getfield(L, 1, "on_header");
	lua_setfield(L, -2, "on_header");

	lua_getfield(L, 1, "done");
	lua_setfield(L, -2, "done");

	debug("starts");

	uv_work_t *w = (uv_work_t *)zalloc(sizeof(uv_work_t));
	w->data = lc;
	uv_queue_work(loop, w, curl_thread, curl_thread_done);

	lua_pushvalue(L, -1);
	lua_setglobalptr(L, "curl", lc);

	return 1;
}

void luv_curl_init(lua_State *L, uv_loop_t *loop) {
	// curl = [native function]
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, curl, 1);
	lua_setglobal(L, "curl");
}

