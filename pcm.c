
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include <uv.h>
#include <lua.h>

#include "prof.h"
#include "pcm.h"
#include "utils.h"

static int32_t tblvals[101] = {
	0,4,6,7,9,11,13,16,19,22,26,31,36,42,49,57,65,75,85,97,110,123,139,155,173,192,213,235,259,284,311,340,370,402,436,471,508,546,586,628,671,716,763,811,860,911,963,1016,1071,1126,1183,1241,1299,1359,1420,1481,1543,1605,1668,1732,1796,1860,1924,1989,2054,2119,2184,2248,2313,2378,2442,2506,2570,2633,2696,2759,2821,2883,2944,3004,3064,3123,3181,3239,3296,3352,3407,3462,3516,3569,3621,3672,3723,3772,3821,3869,3916,3962,4007,4052,4096
};
static int32_t tblbase = 4096;

static prof_t pf_dovol = {"pcm.dovol"};
static prof_t pf_domix = {"pcm.domix"};

prof_t *pf_pcm[] = {
	&pf_domix,
	&pf_dovol,
	NULL,
};

void pcm_do_volume(void *_out, int len, float fvol) {
	int16_t *out = (int16_t *)_out;
	len /= 2;

	if (fvol > 1) {
		// pass through
		return;
	} else if (fvol < 0) {
		// set zero
		memset(out, 0, len);
		return;
	}

	prof_inc(&pf_dovol);

	int vi = (int)(fvol*100);
	int32_t a = tblvals[vi];

	while (len--) {
		int32_t v = *out;
		v = v * a / tblbase;
		*out = (int16_t)v;
		out++;
	}
}

void pcm_do_mix(void *_out, void *_in, int len) {
	prof_inc(&pf_domix);

	int16_t *out = (int16_t *)_out;
	int16_t *in = (int16_t *)_in;

	len /= 2;
	while (len--) {
		*out += *in;
		out++;
		in++;
	}
}

static int lua_pcm_setopt(lua_State *L) {

	lua_getfield(L, 1, "tblvals");
	if (lua_isnil(L, -1))
		return 0;

	int i;
	for (i = 1; i <= 101; i++) {
		lua_pushnumber(L, i);
		lua_gettable(L, -2);
		int32_t v = (int32_t)lua_tonumber(L, -1);
		tblvals[i-1] = v;
		lua_pop(L, 1);
	}

	lua_getfield(L, 1, "tblbase");
	if (lua_isnil(L, -1))
		return 0;
	tblbase = lua_tonumber(L, -1);

	info("tblbase=%d", tblbase);

	return 0;
}

void luv_pcm_init(lua_State *L, uv_loop_t *loop) {
	lua_register(L, "pcm_setopt", lua_pcm_setopt);
}

