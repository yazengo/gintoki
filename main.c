#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "tests.h"
#include "utils.h"
#include "strbuf.h"
#include "lua_cjson.h"
#include "luv_curl.h"
#include "blowfish.h"
#include "upnp_device.h"
#include "audio_mixer.h"
#include "audio_in.h"
#include "pcm.h"

static void usage(char *prog) {
	fprintf(stderr, "Usage: %s\n", prog);
	fprintf(stderr, "   -test-c 101                      run C test #101          \n");
	fprintf(stderr, "   -run a.lua b.lua ...             run lua script one by one\n");
	exit(-1);
}

int main(int argc, char *argv[]) {

	utils_preinit();

	int test_c = -1;
	char **run_lua = NULL;

	int i;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-h")) usage(argv[0]);
		if (!strcmp(argv[i], "-test-c")) {
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

	uv_loop_t *loop = uv_default_loop();
	uv_async_t as;
	uv_async_init(loop, &as, NULL); // make loop never exists

	lua_State *L = luaL_newstate();

	luaL_openlibs(L);
	luaopen_cjson_safe(L);

	utils_init(L, loop);
	lua_dofile_or_die(L, "utils.lua");

	pcm_init();
	audio_mixer_init(L, loop);

#ifdef USE_CURL
	luv_curl_init(L, loop);
#endif

#ifdef USE_INPUTDEV
	luv_inputdev_init(L, loop);
#endif

#ifdef USE_AIRPLAY
	if (getenv("ENABLE_AIRPLAY"))
		luv_airplay_init(L, loop);
#endif

	luv_blowfish_init(L, loop);

	if (getenv("DISABLE_UPNP") == NULL) 
		upnp_init(L, loop);

	if (test_c >= 200 && test_c < 300) {
		run_test_c_post(test_c-200, L, loop);
	}

	float tm_start = now();
	if (run_lua) {
		while (*run_lua) {
			lua_dofile_or_die(L, *run_lua);
			run_lua++;
		}
	}
	info("scripts loaded in %.f ms", (now()-tm_start)*1e3);

	return uv_run(loop, UV_RUN_DEFAULT);
}

