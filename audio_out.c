
#include <unistd.h>
#include <ao/ao.h>

#include "utils.h"
#include "audio_out.h"

// on thread main
static void play_done(uv_work_t *w, int stat) {
	audio_out_t *ao = (audio_out_t *)w->data;

	ao->play_buf = NULL;

	if (ao->on_play_done)
		ao->on_play_done(ao, ao->play_len);

	free(w);
}

// on thread play
static void play_thread(uv_work_t *w) {
	audio_out_t *ao = (audio_out_t *)w->data;

	//usleep(1e6 * (ao->play_len / (44100*4.0)));
	ao_play(ao->aodev, ao->play_buf, ao->play_len);
}

// on thread main
void audio_out_play(audio_out_t *ao, void *buf, int len, void (*done)(audio_out_t *, int)) {
	if (ao->play_buf) {
		return;
	}
	ao->play_buf = buf;
	ao->play_len = len;
	ao->on_play_done = done;

	uv_work_t *w = (uv_work_t *)zalloc(sizeof(uv_work_t));
	w->data = ao;
	uv_queue_work(ao->loop, w, play_thread, play_done);
}

void audio_out_cancel_play(audio_out_t *ao) {
	ao->on_play_done = NULL;
}

void audio_out_set_rate(audio_out_t *ao, int rate) {
	ao_sample_format fmt = {};
	fmt.bits = 16;
	fmt.channels = 2;
	fmt.rate = rate;
	fmt.byte_format = AO_FMT_LITTLE;

	if (ao->aodev)
		ao_close(ao->aodev);

	int drv = ao_default_driver_id();
	ao->aodev = ao_open_live(drv, &fmt, NULL);
}

void audio_out_init(uv_loop_t *loop, audio_out_t *ao, int rate) {
	ao->loop = loop;

	ao_initialize();
	audio_out_set_rate(ao, 44100);
}

