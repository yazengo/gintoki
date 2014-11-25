
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "utils.h"
#include "cjson.h"
#include "tests.h"

static void usage(char *prog) {
	fprintf(stderr, "Usage: %s [lua files]\n", prog);
	fprintf(stderr, "   -t 101        run test #101\n");
	fprintf(stderr, "   -name=value   export name=value\n");
	fprintf(stderr, "   -name         export name=1\n");
	exit(-1);
}

int main(int argc, char *argv[]) {
	uv_loop_t *loop = uv_default_loop();

	int i;
	for (i = 1; i < argc; i++) {
		char *a = argv[i];
		if (!strcmp(a, "-h")) 
			usage(argv[0]);
		else if (!strcmp(a, "-v")) {
			printf("build %s\n", BUILDDATE);
			printf("version %s\n", GITVER);
			return 0;
		} else if (a[0] == '-') {
			char *eq = strstr(a, "=");
			char *env;
			if (eq) {
				env = strdup(a+1);
			} else {
				env = strcat(strdup(a+1), "=1");
			}
			putenv(env);
		}
	}

	utils_preinit(loop);
	info("starts");

	lua_State *L = luaL_newstate();

	lua_pushstring(L, BUILDDATE);
	lua_setglobal(L, "builddate");

	luaL_openlibs(L);
	luaopen_cjson_safe(L);

	LUVINIT;

	lua_dofile_or_die(L, "utils.lua");

	float tm_start = now();
	int n = 0;
	for (i = 1; i < argc; i++) {
		char *a = argv[i];
		if (!strcmp(a, "-t")) {
			int t = -1;
			if (i+1 >= argc) usage(argv[0]);
			sscanf(argv[i+1], "%d", &t);
			if (t != -1) {
				run_test(t, L, loop, &argv[i+2]);
				n++;
			}
			i++;
		} else if (a[0] == '-') {
			// already handled
		} else if (strlen(a) > 4 && !strcmp(a+strlen(a)-4, ".lua")) {
			lua_dofile_or_die(L, a);
			n++;
		} else {
			lua_dostring_or_die(L, a);
			n++;
		}
	}
	if (n == 0)
		usage(argv[0]);

	info("scripts loaded in %.f ms", (now()-tm_start)*1e3);
	
	uv_run(loop, UV_RUN_DEFAULT);
	lua_close(L);
	
	return 0;
}

