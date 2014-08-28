
#include "utils.h"
#include "audio_in.h"
#include "audio_out_test.h"

static void read(audio_in_t *ai, void *buf, int len) {
	audio_out_test_fill_buf_with_key(buf, len, 22050, 3);
	info("len=%d", len);
	ai->on_read_done(ai, len);
	ai->on_read_done = NULL;
}

static void stop(audio_in_t *ai) {
}

void audio_in_airplay_init(uv_loop_t *loop, audio_in_t *ai) {
	ai->read = read;
	ai->stop = stop;
	ai->on_start(ai, 22050);
}

