#include <stdio.h>
#include <uv.h>
#include <math.h>
#include <stdint.h>

#include "utils.h"
#include "audio_out.h"

static int16_t buf[44100/3];
static float freq[] = {
	261.63, 293.66, 329.63, 349.23, 392.00,
	440.00, 493.88,
};
static uint32_t freq_i;

static void fillbuf(int16_t *buf, int n) {
	int i;
	float T = 1.0/freq[freq_i%7];
	int rate = 44100;

	for (i = 0; i < n; i++) {
		float t = i*1.0/rate;
		float t_sin = t - floor(t/T)*T;
		buf[i] = sinf(t_sin/T * 2*M_PI) * 5000;
	}
}

static void done(audio_out_t *ao) {
	info("freq %d", freq_i);
	freq_i++;
	fillbuf(buf, sizeof(buf)/2);
	audio_out_play(ao, buf, sizeof(buf), done);
}

void test_audio_out(uv_loop_t *loop) {
	audio_out_t *ao = (audio_out_t *)malloc(sizeof(audio_out_t));

	audio_out_init(loop, ao, 44100);

	fillbuf(buf, sizeof(buf)/2);
	audio_out_play(ao, buf, sizeof(buf), done);
}

