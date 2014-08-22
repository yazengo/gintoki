
#include <stdint.h>
#include "pcm.h"

static inline int16_t clip_int16_c(int a) {
    if ((a+0x8000) & ~0xFFFF) return (a>>31) ^ 0x7FFF;
    else                      return a;
}

void pcm_do_volume(void *_out, int len, float fvol) {
	int16_t *out = (int16_t *)_out;
	len /= 2;

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


