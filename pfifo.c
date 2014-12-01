
#include <stdlib.h>
#include <string.h>

#include "luv.h"
#include "pipe.h"
#include "utils.h"

static void open_done(fs_req_t *req) {
	int fd = req->fd;
	if (fd == -1) 
		panic("open %s failed", req->path);

	pipe_t *p = (pipe_t *)req->data;
	debug("p=%p");

	fs_req_cleanup(req);
	free(req);

	uv_pipe_init(luv_loop(p), &p->p, 0);
	uv_pipe_open(&p->p, fd);

	p->type = PSTREAM_SRC;
	p->st = (uv_stream_t *)&p->p;

	lua_State *L = luv_state(p);
	luv_pushctx(L, p);
	luv_callfield(p, "open_cb", 1, 0);
}

static int pfifo_open(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	char *path = (char *)lua_tostring(L, 1);

	if (path == NULL)
		panic("path must be set");

	debug("p=%p path=%s", p, path);

	lua_pushvalue(L, 2);
	lua_setfield(L, -2, "open_cb");

	fs_req_t *req = (fs_req_t *)zalloc(sizeof(fs_req_t));
	req->data = p;
	req->path = strdup(path);
	fs_open(loop, req, open_done);

	return 0;
}

void luv_pfifo_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pfifo_open", pfifo_open);
}

