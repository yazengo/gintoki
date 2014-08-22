#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>

#include "tests.h"
#include "utils.h"
#include "strbuf.h"
#include "upnp_device.h"
#include "upnp_util.h"

static void usage(char *prog) {
	fprintf(stderr, "Usage: %s\n", prog);
	fprintf(stderr, "   -test-c 101                      run C test #101              \n");
	fprintf(stderr, "   -test-lua a.lua b.lua ...        run lua test one by one      \n");
	exit(-1);
}

int main(int argc, char *argv[]) {
	int test_c = -1;
	char **test_lua = NULL;

	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h")) usage(argv[0]);
		if (!strcmp(argv[i], "-test-c")) {
			if (i+1 >= argc) usage(argv[0]);
			sscanf(argv[i+1], "%d", &test_c);
			break;
		}
		if (!strcmp(argv[i], "-test-lua")) {
			if (i+1 >= argc) usage(argv[0]);
			test_lua = &argv[i+1];
			break;
		}
	}

	info("starts");

	if (test_c >= 100 && test_c < 200) {
		run_test_c_pre(test_c);
		return 0;
	}

	uv_loop_t *loop = uv_default_loop();
	uv_async_t as;
	uv_async_init(loop, &as, NULL); // make loop never exists

	lua_State *L = luaL_newstate();

	luaL_openlibs(L);
	luaopen_cjson_safe(L);

	utils_init(L, loop);
	lua_dofile_or_die(L, "utils.lua");

	audio_mixer_init(L, loop);
	upnp_init(L, loop);

	lua_dofile_or_die(L, "radio.lua");

	if (test_c >= 200 && test_c < 300) {
		run_test_c_post(test_c-200, L, loop);
		return uv_run(loop, UV_RUN_DEFAULT);
	}

	if (test_lua) {
		while (*test_lua) {
			lua_dofile_or_die(L, *test_lua);
			test_lua++;
		}
		return uv_run(loop, UV_RUN_DEFAULT);
	}

	lua_dofile_or_die(L, "main.lua");

	return uv_run(loop, UV_RUN_DEFAULT);
}

