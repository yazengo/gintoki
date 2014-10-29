
#include <unistd.h>
#include <errno.h>
#include <sys/soundcard.h>

#include "utils.h"
#include "audio_out.h"

typedef struct {
	int fd_oss;
} jzcodec_t;

static void jzcodec_set_rate(audio_out_t *ao, int rate) {
	jzcodec_t *jz = (jzcodec_t *)ao->out;
	int v, r;

	info("init");

	if (jz->fd_oss)
		close(jz->fd_oss);

	jz->fd_oss = open("/dev/dsp", O_WRONLY);
	if (jz->fd_oss < 0) 
		panic("open dev failed: %s", strerror(errno));

	v = AFMT_S16_LE;
	r = ioctl(jz->fd_oss, SNDCTL_DSP_SETFMT, &v);
	if (r < 0) 
		panic("ioctl setfmt failed: %s", strerror(errno));

	v = 12|(8<<16);
	r = ioctl(jz->fd_oss, SNDCTL_DSP_SETFRAGMENT, &v);
	if (r < 0) 
		panic("ioctl setfragment: %s", strerror(errno));

	v = 2;
	r = ioctl(jz->fd_oss, SNDCTL_DSP_CHANNELS, &v);
	if (r < 0) 
		panic("ioctl set channels failed: %s", strerror(errno));

	v = rate;
	r = ioctl(jz->fd_oss, SNDCTL_DSP_SPEED, &v);
	if (r < 0) 
		panic("ioctl set speed failed: %s", strerror(errno));

	if (v != rate) 
		panic("driver sample_rate changes: orig=%d ret=%d", rate, v);

	info("done");
}

static void jzcodec_play(audio_out_t *ao, void *buf, int len) {
	jzcodec_t *jz = (jzcodec_t *)ao->out;
	write(jz->fd_oss, buf, len);
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
	jzcodec_t *jz = (jzcodec_t *)zalloc(sizeof(jzcodec_t));

	ao->out = jz;
	ao->loop = loop;

	ao->set_rate = jzcodec_set_rate;
	ao->play = jzcodec_play;

	ao->set_rate(ao, rate);
	info("done");
}

