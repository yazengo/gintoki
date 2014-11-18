
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

	int fd;

	if (mode[0] == 'r')
		fd = open(filename, O_RDONLY);
	else
		fd = open(filename, O_WRONLY|O_CREAT, 0777);

	if (fd == -1) {
		info("open %s mode %s failed", filename, mode);
		lua_pushnil(L);
		return 1;
	}

	lua_newtable(L);

	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	uv_pipe_init(loop, &p->p, 0);
	uv_pipe_open(&p->p, fd);
	p->st = (uv_stream_t *)&p->p;

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

