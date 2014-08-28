
#include "audio_in.h"

static void read(audio_in_t *ai, void *buf, int len) {
	ai->on_read_done(ai, len);
}

void audio_in_airplay_init(uv_loop_t *loop, audio_in_t *ai) {
}

