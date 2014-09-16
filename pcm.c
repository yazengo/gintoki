
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "pcm.h"
#include "utils.h"

static inline int16_t clip_int16_c(int a) {
    if ((a+0x8000) & ~0xFFFF) return (a>>31) ^ 0x7FFF;
    else                      return a;
}

enum {
	DIV, SHIFT8,
};
static int mode;

/*
  {{1,17.4}, {2,37.2}, {3,72.4},
  {4,127.8}, {5,226.5}, {6,396}, {7,627},
  {8,994}, {9,1560}, {10,2475}, {11,3910}, 
  {12,5530}, {13,8760}, {14,12400}, {15,19450}}
*/

static float divtbl[16] = {
	1, 17.4, 37.2, 72.4,
	127.8, 226.5, 396, 627,
	994, 1560, 2475, 3910, 
	5530, 8760, 12400, 19450,
};

void pcm_do_volume(void *_out, int len, float fvol) {
	int16_t *out = (int16_t *)_out;
	len /= 2;

	if (mode == DIV) {
		int div = (int)powf(10.0, (1.0 - fvol) * 45.0 / 10.0);

		while (len--) {
			*out = *out / div;
			out++;
		}
	} else {
		int vol = fvol * 255;
		if (vol == 255)
			return;
		if (vol > 255)
			vol = 255;
		if (vol <= 0)
			vol = 0;
		while (len--) {
			*out = clip_int16_c((*out * vol) >> 8);
			out++;
		}
	}
}

void pcm_do_mix(void *_out, void *_in, int len) {
	int16_t *out = (int16_t *)_out;
	int16_t *in = (int16_t *)_in;

	len /= 2;
	while (len--) {
		*out += *in;
		out++;
		in++;
	}
}

void pcm_init() {
	if (getenv("VOL_DIV") == NULL) {
		mode = SHIFT8;
		info("vol: use shift8");
	} else {
		mode = DIV;
		info("vol: use div");
	}
}

