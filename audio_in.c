
#include <string.h>
#include <stdlib.h>

#include "utils.h"
#include "audio_in.h"

void audio_in_avconv_init(uv_loop_t *loop, audio_in_t *ai);
void audio_in_airplay_init(uv_loop_t *loop, audio_in_t *ai);
void audio_in_airplay_v2_init(uv_loop_t *loop, audio_in_t *ai);
void audio_in_noise_init(uv_loop_t *loop, audio_in_t *ai);

static void error_read(audio_in_t *ai, void *buf, int len, audio_in_read_cb done) {
	panic("should not call this");
}

static void error_stop_read(audio_in_t *ai) {
}

static void error_close(audio_in_t *ai, audio_in_close_cb done) {
	done(ai);
}

void audio_in_error_init(uv_loop_t *loop, audio_in_t *ai, const char *err) {
	ai->read = error_read;
	ai->stop_read = error_stop_read;
	ai->close = error_close;
}

void audio_in_init(uv_loop_t *loop, audio_in_t *ai) {
	char *airplay = "airplay://";
	char *noise = "noise://";

	if (!strncmp(ai->url, airplay, strlen(airplay))) {
		if (getenv("AIRPLAY_V1"))
			audio_in_airplay_init(loop, ai);
		else
			audio_in_airplay_v2_init(loop, ai);
	} else if (!strncmp(ai->url, noise, strlen(noise))) {
		audio_in_noise_init(loop, ai);
	} else
		audio_in_avconv_init(loop, ai);
}

