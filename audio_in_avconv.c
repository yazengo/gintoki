
#include <stdlib.h>

#include "utils.h"
#include "audio_in.h"
#include "avconv.h"

static void avconv_on_free(avconv_t *av) {
	audio_in_t *ai = (audio_in_t *)av->data;
	free(av);
	ai->on_free(ai);
}

static void avconv_on_exit(avconv_t *av) {
	audio_in_t *ai = (audio_in_t *)av->data;
	ai->on_exit(ai);
}

static void avconv_on_probe(avconv_t *av, const char *key, void *val) {
	audio_in_t *ai = (audio_in_t *)av->data;
	ai->on_probe(ai, key, val);
}

static void avconv_on_read_done(avconv_t *av, int n) {
	audio_in_t *ai = (audio_in_t *)av->data;
	ai->on_read_done(ai, n);
	ai->on_read_done = NULL;
}

static void read(struct audio_in_s *ai, void *buf, int len) {
	avconv_t *av = (avconv_t *)ai->in;
	avconv_read(av, buf, len, avconv_on_read_done);
}

static void stop(audio_in_t *ai) {
	avconv_t *av = (avconv_t *)ai->in;
	avconv_stop(av);
}

void audio_in_avconv_init(uv_loop_t *loop, audio_in_t *ai) {
	avconv_t *av = (avconv_t *)zalloc(sizeof(avconv_t));
	av->data = ai;
	ai->in = av;

	ai->read = read;
	ai->stop = stop;

	av->on_probe = avconv_on_probe;
	av->on_exit = avconv_on_exit;
	av->on_free = avconv_on_free;

	ai->on_start(ai, 44100);
	avconv_start(loop, av, ai->url);
}

