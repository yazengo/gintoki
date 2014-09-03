
#include <fcntl.h>
#include <stdio.h>
#include <curl/curl.h>
#include <uv.h>

#include "utils.h"
#include "strbuf.h"

//
// c = curl {
// 	url = 'xx',
// 	retfile = 'x',
// 	reqstr = '',
// 	done = function (content, st) 
// 	end,
// }
//
// c.stat() == {
// 	stat='downloading/done/cancelled',
// 	code=200, progress=33.33, rx=12312, time=33.33, speed=1232
// 	size=1232.33,
// }
// c.cancel()
//

typedef struct {
	CURL *c;

	int stat;
	const char *err;

	char *reqstr;
	int reqstr_len;

	char *retfname;
	FILE *retfp;
	strbuf_t *retsb;

	long code;
	double size;
	double progress;
	double rx;
	double time;
	double speed;

	lua_State *L;
} luv_curl_t;

enum {
	DOWNLOADING,
	CANCELLED,
	DONE,
	ERROR,
};

static void getinfo(luv_curl_t *lc) {
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
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *data) {
	luv_curl_t *lc = (luv_curl_t *)data;

	if (lc->stat == CANCELLED)
		return 0;

	size *= nmemb;

	if (lc->retfp) 
		size = fwrite(ptr, 1, size, lc->retfp);
	if (lc->retsb)
		strbuf_append_mem(lc->retsb, ptr, size);

	debug("write=%d", size);

	return size;
}

static size_t read_data(void *ptr, size_t size, size_t nmemb, void *data) {
	luv_curl_t *lc = (luv_curl_t *)data;

	if (lc->stat == CANCELLED)
		return 0;

	size *= nmemb;
	if (size > lc->reqstr_len)
		size = lc->reqstr_len;

	memcpy(ptr, lc->reqstr, size);
	lc->reqstr_len -= size;
	lc->reqstr += size;

	return size;
}

static void push_curl_stat(luv_curl_t *lc) {
	lua_State *L = lc->L;

	char *stat = "?";
	switch (lc->stat) {
	case DOWNLOADING: stat = "downloading"; break;
	case DONE: stat = "done"; break;
	case CANCELLED: stat = "cancelled"; break;
	case ERROR: stat = "err"; break;
	}

	// return {
	// 	stat='downloading/done/cancelled',
	// 	code=200, progress=33.33, rx=12312, time=33.33, speed=1232
	// 	size=1232.33,
	// }

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

	if (lc->stat == ERROR) {
		lua_pushstring(L, lc->err);
		lua_setfield(L, -2, "err");
	}

	if (lc->retfname) {
		lua_pushstring(L, lc->retfname);
		lua_setfield(L, -2, "retfname");
	}
}

static int curl_stat(lua_State *L) {
	luv_curl_t *lc = (luv_curl_t *)lua_touserdata(L, lua_upvalueindex(1));

	if (lc->stat == DOWNLOADING)
		getinfo(lc);
	push_curl_stat(lc);

	return 1;
}

static int curl_cancel(lua_State *L) {
	luv_curl_t *lc = (luv_curl_t *)lua_touserdata(L, lua_upvalueindex(1));

	lc->stat = CANCELLED;

	return 0;
}

static void curl_thread_done(uv_work_t *w, int _) {
	luv_curl_t *lc = (luv_curl_t *)w->data;
	lua_State *L = lc->L;

	info("done");

	free(w);

	if (lc->stat == DOWNLOADING)
		lc->stat = DONE;

	// 1
	if (lc->retsb) {
		//debug("len=%d %s", strlen(lc->retsb->buf), lc->retsb->buf);
		strbuf_append_char(lc->retsb, 0);
		lua_pushstring(L, lc->retsb->buf);
	} else 
		lua_pushnil(L);

	if (lc->retfp)
		fclose(lc->retfp);

	// 2
	getinfo(lc);
	push_curl_stat(lc);

	lua_do_global_callback(lc->L, "curl_done", lc->c, 2, 1);

	curl_easy_cleanup(lc->c);
	if (lc->retsb)
		strbuf_free(lc->retsb);
	if (lc->retfname)
		free(lc->retfname);
}

static void curl_thread(uv_work_t *w) {
	luv_curl_t *lc = (luv_curl_t *)w->data;

	if (lc->retfname) {
		lc->retfp = fopen(lc->retfname, "wb+");
		info("'%s' opened", lc->retfname);
		if (lc->retfp == NULL) {
			warn("open '%s' failed", lc->retfname);
			lc->stat = ERROR;
			lc->err = "open_file_failed";
			return;
		}
	}

	CURLcode r = curl_easy_perform(lc->c);

	debug("thread ends r=%d", r);
}

static int curl(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	luv_curl_t *lc = (luv_curl_t *)lua_newuserdata(L, sizeof(luv_curl_t)); // 2
	memset(lc, 0, sizeof(luv_curl_t));

	lc->L = L;

	lua_getfield(L, 1, "url"); // 3
	lua_getfield(L, 1, "retfile"); // 4
	lua_getfield(L, 1, "reqstr"); // 5
	lua_getfield(L, 1, "done"); // 6

	char *url = (char *)lua_tostring(L, 3);
	info("url=%s", url);
	if (url == NULL) 
		return 0;

	char *retfname = (char *)lua_tostring(L, 4);
	if (retfname)
		lc->retfname = strdup(retfname);
	else
		lc->retsb = strbuf_new(2048);

	lc->c = curl_easy_init();

	curl_easy_setopt(lc->c, CURLOPT_URL, url);
	curl_easy_setopt(lc->c, CURLOPT_WRITEFUNCTION, write_data);
	curl_easy_setopt(lc->c, CURLOPT_WRITEDATA, lc);

	lc->reqstr = (char *)lua_tostring(L, 5);
	if (lc->reqstr) {
		lc->reqstr_len = strlen(lc->reqstr);
		curl_easy_setopt(lc->c, CURLOPT_READFUNCTION, read_data);
		curl_easy_setopt(lc->c, CURLOPT_READDATA, lc);
		curl_easy_setopt(lc->c, CURLOPT_UPLOAD, 1);
	}

	lua_pushvalue(L, 6);
	lua_set_global_callback(L, "curl_done", lc->c);

	// return {
	// 		cancel = [native code],
	// 		stat = [native code],
	// }
	lua_newtable(L);

	lua_pushvalue(L, 2);
	lua_pushcclosure(L, curl_cancel, 1);
	lua_setfield(L, -2, "cancel");

	lua_pushvalue(L, 2);
	lua_pushcclosure(L, curl_stat, 1);
	lua_setfield(L, -2, "stat");

	debug("thread starts");

	uv_work_t *w = (uv_work_t *)zalloc(sizeof(uv_work_t));
	w->data = lc;
	uv_queue_work(loop, w, curl_thread, curl_thread_done);

	return 1;
}

void luv_curl_init(lua_State *L, uv_loop_t *loop) {
	// curl = [native code]
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, curl, 1);
	lua_setglobal(L, "curl");
}

