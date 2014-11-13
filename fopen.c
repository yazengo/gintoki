
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"

static void on_closed(uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;

	info("%p", p);

	uv_close((uv_handle_t *)p->st, NULL);
}

// fopen(filename, mode)
// { stdout = [pipe_t] }
static int luv_fopen(lua_State *L, uv_loop_t *loop) {
	char *filename = (char *)lua_tostring(L, 1);
	char *mode = (char *)lua_tostring(L, 2);

	if (filename == NULL)
		panic("filename must be set");
	if (mode == NULL)
		mode = "r";

	uv_file fd; 
	uv_fs_t req;
	int r;

	if (mode[0] == 'r')
		r = uv_fs_open(loop, &req, filename, O_RDONLY, 0644, NULL);
	else
		r = uv_fs_open(loop, &req, filename, O_WRONLY|O_CREAT, 0644, NULL);

	fd = req.result;
	uv_fs_req_cleanup(&req);

	if (r) {
		info("open %s mode %s failed", filename, mode);
		lua_pushnil(L);
		return 1;
	}

	lua_newtable(L);

	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	p->type = PT_FILE;
	p->fd = fd; 

	luv_setgc(p, on_closed);

	if (mode[0] == 'r')
		lua_setfield(L, -2, "stdout");
	else
		lua_setfield(L, -2, "stdin");

	return 1;
}

void luv_fopen_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "fopen", luv_fopen);
}

