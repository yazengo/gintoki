
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "config.h"
#include "cjson.h"
#include "tests.h"

static void usage(char *prog) {
	fprintf(stderr, "Usage: %s\n", prog);
	fprintf(stderr, "   -t 101                           run C test #101          \n");
	fprintf(stderr, "   -run a.lua b.lua ...             run lua script one by one\n");
	exit(-1);
}

int main(int argc, char *argv[]) {
	uv_loop_t *loop = uv_default_loop();

	setenv("_", argv[0], 1);
	utils_preinit(loop);

	int test_c = -1;
	char **run_lua = NULL;

	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h")) usage(argv[0]);
		if (!strcmp(argv[i], "-v")) {
			puts(GITVER);
			return 0;
		}
		if (!strcmp(argv[i], "-t")) {
			if (i+1 >= argc) usage(argv[0]);
			sscanf(argv[i+1], "%d", &test_c);
			i++;
			continue;
		}
		if (!strcmp(argv[i], "-run")) {
			if (i+1 >= argc) usage(argv[0]);
			run_lua = &argv[i+1];
			break;
		}
	}

	info("starts");

	if (test_c >= 100 && test_c < 200) {
		run_test_c_pre(test_c-100);
		return 0;
	}

	uv_async_t as;
	uv_async_init(loop, &as, NULL); // make loop never exists

	lua_State *L = luaL_newstate();

	luaL_openlibs(L);
	luaopen_cjson_safe(L);

	LUVMOD_INIT;

	if (test_c >= 200 && test_c < 300) {
		run_test_c_post(test_c-200, L, loop, argv);
	}

	lua_dofile_or_die(L, "utils.lua");

	float tm_start = now();
	if (run_lua) {
		while (*run_lua) {
			char *cmd = *run_lua;
			if (strlen(cmd) > 4 && !strcmp(cmd+strlen(cmd)-4, ".lua"))
				lua_dofile_or_die(L, cmd);
			else
				lua_dostring_or_die(L, cmd);
			run_lua++;
		}
	}
	info("scripts loaded in %.f ms", (now()-tm_start)*1e3);

	return uv_run(loop, UV_RUN_DEFAULT);
}

