#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>

#include "utils.h"
#include "strbuf.h"
#include "upnp_device.h"
#include "upnp_util.h"

static void usage() {
	exit(-1);
}

int main(int argc, char *argv[]) {
	info("starts");

	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-he")) {
			if (i+1 >= argc) usage();
			int no = 0; sscanf(argv[i+1], "%d", &no);
			run_hello(no);
			return 0;
		}
	}

	uv_loop_t *loop = uv_default_loop();
	uv_async_t as;
	uv_async_init(loop, &as, NULL); // make loop never exists

	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	luaopen_cjson_safe(L);
	luaL_dofile(L, "utils.lua");

	upnp_init(L, loop);
	luaL_dofile(L, "main.lua");

	uv_run(loop, UV_RUN_DEFAULT);

	return 0;
}

