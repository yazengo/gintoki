
#include "luv.h"
#include "pipe.h"

typedef struct {
} pfilebuf_t;

/*
pipe.copy(audio.noise(), pipe.filebuf('a.pcm'), audio.out())
*/

static int pfilebuf(lua_State *L, uv_loop_t *loop) {
	lua_newtable(L);

	lua_pushnumber(L, 1);
	pipe_t *src = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	lua_settable(L, -3);

	lua_pushnumber(L, 2);
	pipe_t *sink = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	lua_settable(L, -3);

	return 1;
}

void luv_pfilebuf_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pfilebuf", pfilebuf);
}

