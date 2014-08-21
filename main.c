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

	int hello = 0;

	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-hello")) {
			if (i+1 >= argc)
				usage();
			sscanf(argv[i+1], "%d", &hello);
		}
	}

	if (hello >= 100 && hello < 200) {
		run_hello(hello);
		return 0;
	}

	uv_loop_t *loop = uv_default_loop();
	uv_async_t as;
	uv_async_init(loop, &as, NULL); // make loop never exists

	lua_State *L = luaL_newstate();

	luaL_openlibs(L);
	luaopen_cjson_safe(L);
	luaL_dofile(L, "utils.lua");
	utils_init(L, loop);

	if (hello >= 200 && hello < 300) {
		run_test_c(hello-200, L, loop);
		return uv_run(loop, UV_RUN_DEFAULT);
	}

	if (hello >= 300 && hello < 400) {
		run_test_lua(hello-300, L, loop);
		return uv_run(loop, UV_RUN_DEFAULT);
	}

	upnp_init(L, loop);

	luaL_dofile(L, "main.lua");

	return uv_run(loop, UV_RUN_DEFAULT);
}

