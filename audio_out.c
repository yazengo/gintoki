
#include <unistd.h>
#include <errno.h>

#ifdef USE_JZCODEC
#include <sys/soundcard.h>
#endif

#include "utils.h"
#include "audio_out.h"

#ifndef USE_JZCODEC

#include <ao/ao.h>

static void libao_init(audio_out_t *ao) {
	ao_initialize();
}

static void libao_set_rate(audio_out_t *ao, int rate) {
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

static void libao_play(audio_out_t *ao, void *buf, int len) {
	ao_play(ao->aodev, buf, len);
}

#endif

static void dummy_play(audio_out_t *ao, void *buf, int len) {
	usleep(1e6 * (len / (ao->rate*4.0)));
}

#ifdef USE_JZCODEC
static void jzcodec_init(audio_out_t *ao) {
}

static void jzcodec_set_rate(audio_out_t *ao, int rate) {
	int v, r;

	info("init");

	if (ao->fd_oss)
		close(ao->fd_oss);

	ao->fd_oss = open("/dev/dsp", O_WRONLY);
	if (ao->fd_oss < 0) 
		panic("open dev failed: %s", strerror(errno));

	v = AFMT_S16_LE;
	r = ioctl(ao->fd_oss, SNDCTL_DSP_SETFMT, &v);
	if (r < 0) 
		panic("ioctl setfmt failed: %s", strerror(errno));

	v = 12|(8<<16);
	r = ioctl(ao->fd_oss, SNDCTL_DSP_SETFRAGMENT, &v);
	if (r < 0) 
		panic("ioctl setfragment: %s", strerror(errno));

	v = 2;
	r = ioctl(ao->fd_oss, SNDCTL_DSP_CHANNELS, &v);
	if (r < 0) 
		panic("ioctl set channels failed: %s", strerror(errno));

	v = rate;
	r = ioctl(ao->fd_oss, SNDCTL_DSP_SPEED, &v);
	if (r < 0) 
		panic("ioctl set speed failed: %s", strerror(errno));

	if (v != rate) 
		panic("driver sample_rate changes: orig=%d ret=%d", rate, v);

	info("done");
}

static void jzcodec_play(audio_out_t *ao, void *buf, int len) {
	write(ao->fd_oss, buf, len);
}
#endif

int audio_out_is_playing(audio_out_t *ao) {
	return ao->is_playing;
}

// on thread main
static void play_done(uv_work_t *w, int stat) {
	audio_out_t *ao = (audio_out_t *)w->data;

	debug("playdone cb=%p", ao->on_play_done);
	ao->is_playing = 0;

	if (ao->on_play_done)
		ao->on_play_done(ao, ao->play_len);
	debug("playdone end");
}

// on thread play
static void play_thread(uv_work_t *w) {
	audio_out_t *ao = (audio_out_t *)w->data;

	debug("start %p %d", ao->play_buf, ao->play_len);
	ao->play(ao, ao->play_buf, ao->play_len);
	debug("done %p %d", ao->play_buf, ao->play_len);
}

// on thread main
void audio_out_play(audio_out_t *ao, void *buf, int len, void (*done)(audio_out_t *, int)) {
	if (audio_out_is_playing(ao))
		panic("playing not end");

	ao->play_buf = buf;
	ao->play_len = len;
	ao->on_play_done = done;

	ao->is_playing = 1;

	debug("playlen=%d", len);

	static uv_work_t w;
	w.data = ao;
	uv_queue_work(ao->loop, &w, play_thread, play_done);
}

void audio_out_cancel_play(audio_out_t *ao) {
	info("canceled");
	ao->on_play_done = NULL;
}

void audio_out_set_rate(audio_out_t *ao, int rate) {
	info("rate=%d", rate);

	ao->set_rate(ao, rate);
	ao->rate = rate;
}

void audio_out_init(uv_loop_t *loop, audio_out_t *ao, int rate) {
	ao->loop = loop;

#ifdef USE_JZCODEC
	ao->init = jzcodec_init;
	ao->set_rate = jzcodec_set_rate;
	ao->play = jzcodec_play;
#else
	ao->init = libao_init;
	ao->set_rate = libao_set_rate;
	ao->play = libao_play;
#endif

	//ao->play = dummy_play;

	ao->init(ao);
	audio_out_set_rate(ao, rate);
	info("done");
}

