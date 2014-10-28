
#include <unistd.h>
#include <errno.h>

#include "utils.h"
#include "audio_out.h"

#include <ao/ao.h>

typedef struct {
	ao_device *dev;
} libao_t;

static void libao_init(audio_out_t *ao) {
	ao_initialize();
}

static void libao_set_rate(audio_out_t *ao, int rate) {
	libao_t *la = (libao_t *)ao->out;

	ao_sample_format fmt = {};
	fmt.bits = 16;
	fmt.channels = 2;
	fmt.rate = rate;
	fmt.byte_format = AO_FMT_LITTLE;

	if (la->dev)
		ao_close(la->dev);

	int drv = ao_default_driver_id();
	la->dev = ao_open_live(drv, &fmt, NULL);
}

static void libao_play(audio_out_t *ao, void *buf, int len) {
	libao_t *la = (libao_t *)ao->out;
	ao_play(la->dev, buf, len);
}

static void play_done(uv_work_t *w, int stat) {
	audio_out_t *ao = (audio_out_t *)w->data;

	ao->is_playing = 0;

	if (ao->on_play_done)
		ao->on_play_done(ao, ao->play_len);
}

static void play_thread(uv_work_t *w) {
	audio_out_t *ao = (audio_out_t *)w->data;

	debug("start len=%d", ao->play_len);
	ao->play(ao, ao->play_buf, ao->play_len);
	debug("end len=%d", ao->play_len);
}

void audio_out_play(audio_out_t *ao, void *buf, int len, void (*done)(audio_out_t *, int)) {
	if (ao->is_playing)
		panic("playing not end");

	ao->play_buf = buf;
	ao->play_len = len;
	ao->on_play_done = done;

	ao->is_playing = 1;

	static uv_work_t w;
	w.data = ao;
	uv_queue_work(ao->loop, &w, play_thread, play_done);
}

void audio_out_init(uv_loop_t *loop, audio_out_t *ao, int rate) {
	libao_t *la = (libao_t *)zalloc(sizeof(libao_t));

	ao->out = la;
	ao->loop = loop;

	ao->init = libao_init;
	ao->set_rate = libao_set_rate;
	ao->play = libao_play;

	ao->init(ao);
	ao->set_rate(ao, rate);
	info("done");
}

