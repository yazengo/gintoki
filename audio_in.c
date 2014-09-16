
#include <string.h>

#include "utils.h"
#include "audio_in.h"

void audio_in_avconv_init(uv_loop_t *loop, audio_in_t *ai);
void audio_in_airplay_init(uv_loop_t *loop, audio_in_t *ai);

static void error_read(audio_in_t *ai, void *buf, int len, audio_in_read_cb done) {
	panic("should not call this");
}

static void error_stop(audio_in_t *ai) {
}

static void error_close(audio_in_t *ai, audio_in_close_cb done) {
	done(ai);
}

static int error_can_read(audio_in_t *ai) {
	return 0;
}

static int error_is_eof(audio_in_t *ai) {
	return 1;
}

void audio_in_error_init(uv_loop_t *loop, audio_in_t *ai, const char *err) {
	ai->read = error_read;
	ai->stop = error_stop;
	ai->close = error_close;
	ai->can_read = error_can_read;
	ai->is_eof = error_is_eof;
}

void audio_in_init(uv_loop_t *loop, audio_in_t *ai) {
	char *airplay = "airplay://";

	if (!strncmp(ai->url, airplay, strlen(airplay))) 
		audio_in_airplay_init(loop, ai);
	else
		audio_in_avconv_init(loop, ai);
}

