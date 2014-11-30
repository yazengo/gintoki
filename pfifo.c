
#include <stdlib.h>

#include "luv.h"
#include "pipe.h"

static void opened(uv_fs_t *req) {
	uv_fs_req_cleanup(req);
	int fd = req->result;
	pipe_t *p = (pipe_t *)req->data;
	free(req);

	debug("p=%p");

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

	debug("p=%p path=%s", p, path);

	lua_pushvalue(L, 2);
	lua_setfield(L, -2, "open_cb");

	uv_fs_t *req = (uv_fs_t *)zalloc(sizeof(uv_fs_t));
	req->data = p;
	uv_fs_open(loop, req, path, O_RDONLY, 0644, opened);

	return 0;
}

void luv_pfifo_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pfifo_open", pfifo_open);
}

