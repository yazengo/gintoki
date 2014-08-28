
#include "audio_in.h"

void audio_in_stop(audio_in_t *ai) {
	ai->stop(ai);
}

int audio_in_is_reading(audio_in_t *ai) {
	return ai->on_read_done != NULL;
}

void audio_in_read(audio_in_t *ai, void *buf, int len, void (*done)(audio_in_t *ai, int len)) {
	ai->on_read_done = done;
	ai->read(ai, buf, len);
}

