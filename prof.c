
#include "luv.h"
#include "prof.h"
#include "utils.h"

extern prof_t *pf_luv[];
extern prof_t *pf_pcm[];
extern prof_t *pf_objpool[];
extern prof_t pf_immediate;

typedef void (*walk_cb)(prof_t *, void *);

static void walk_list(prof_t *list[], walk_cb cb, void *data) {
	prof_t **p = list;
	while (*p) {
		cb(*p, data);
		p++;
	}
}

static void walk(walk_cb cb, void *data) {
	walk_list(pf_luv, cb, data);
	walk_list(pf_pcm, cb, data);
	walk_list(pf_objpool, cb, data);
	cb(&pf_immediate, data);
}

static void clear(prof_t *p, void *data) {
	p->nr = 0;
}

static int prof_clear(lua_State *L, uv_loop_t *loop) {
	walk(clear, NULL);
	return 0;
}

static void collect(prof_t *p, void *data) {
	lua_State *L = (lua_State *)data;
	lua_pushnumber(L, p->nr);
	lua_setfield(L, -2, p->name);
}

static int prof_collect(lua_State *L, uv_loop_t *loop) {
	lua_newtable(L);
	walk(collect, L);
	return 1;
}

void luv_prof_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "prof_clear", prof_clear);
	luv_register(L, loop, "prof_collect", prof_collect);
}

