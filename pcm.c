
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "pcm.h"
#include "utils.h"

static inline int16_t clip_int16_c(int a) {
	//
	// if (a > 0x8000)
	// 	 a = 0x8000;
	// else if (a < -0x7fff)
	// 	 a = -0x7fff;
	// 
  if ((a+0x8000) & ~0xFFFF) return (a>>31) ^ 0x7FFF;
  else                      return a;
}

enum {
	NONE,
	DIV, SHIFT8,
};
static int mode = DIV;

/*
	Apple iOS 7 volume table
	1, 17.4, 37.2, 72.4,
	127.8, 226.5, 396, 627,
	994, 1560, 2475, 3910, 
	5530, 8760, 12400, 19450,
*/

// after interpolation
static float voltbl[100] = {
	3.715, 6.277, 8.7265, 11.104, 13.45, 15.805, 18.2095, 20.704, 
	23.329, 26.125, 29.1325, 32.392, 35.944, 39.9478, 44.3688, 49.1632, 
	54.3474, 59.9374, 65.9496, 72.4, 78.8577, 85.848, 93.4486, 101.738, 
	110.793, 120.693, 131.478, 143.196, 156.012, 170.019, 185.309, 
	201.976, 220.112, 240.417, 262.601, 286.325, 311.558, 338.269, 
	366.427, 396., 424.909, 455.453, 487.883, 522.452, 559.41, 599.009, 
	641.596, 687.504, 736.755, 789.563, 846.139, 906.696, 971.447, 
	1039.17, 1110.98, 1188.12, 1271.08, 1360.38, 1456.52, 1560., 1670.82, 
	1790.07, 1918.33, 2056.18, 2204.18, 2362.92, 2537.18, 2731.12, 
	2935.25, 3148.44, 3369.56, 3597.48, 3831.07, 4040.16, 4241.99, 4456., 
	4687., 4939.79, 5219.19, 5530., 5941.19, 6384.55, 6856.04, 7351.6, 
	7867.19, 8398.75, 8907.33, 9359.2, 9833.8, 10341.3, 10891.7, 11495.2, 
	12161.9, 12902.1, 13725.6, 14642.8, 15663.7, 16798.4, 18057.2, 19450.
};

void pcm_do_volume(void *_out, int len, float fvol) {
	int16_t *out = (int16_t *)_out;
	len /= 2;

	if (mode == DIV) {
		int vi = (int)(fvol*100);

		if (vi > 100)
			vi = 100;
		else if (vi <= 0) {
			memset(out, 0, len*2);
			return;
		}

		int32_t a = voltbl[99];
		int32_t b = voltbl[vi-1];

		while (len--) {
			int32_t v = *out;
			v = v*b / a;
			*out = (int16_t)v;
			out++;
		}
	} else if (mode == SHIFT8) {
		int vol = fvol * 255;
		if (vol == 255)
			return;
		else if (vol > 255)
			vol = 255;
		else if (vol <= 0)
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

