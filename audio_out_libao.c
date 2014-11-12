
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

static const char *libao_strerror(int e) {
	switch (e) {
		case AO_ENODRIVER:
			return "no driver";

		case AO_ENOTLIVE:
			return "not alive";

		case AO_EBADOPTION:
			return "bad option";

		case AO_EOPENDEVICE:
			return "open device";

		case AO_EFAIL:
			return "efail";

		default:
			return "?";
	}
}

static void libao_list_drivers() {
	int n = 0, i;
	ao_info **d = ao_driver_info_list(&n);

	info("avail drvs:");
	for (i = 0; i < n; i++) {
		info("%s: %s", d[i]->short_name, d[i]->name);
	}
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
	if (drv == -1) {
		libao_list_drivers();
		panic("default driver id not found");
	}

	la->dev = ao_open_live(drv, &fmt, NULL);
	if (la->dev == NULL)
		panic("open failed: %s", libao_strerror(errno));
}

static void libao_play(audio_out_t *ao, void *buf, int len) {
	libao_t *la = (libao_t *)ao->out;
	int r = ao_play(la->dev, buf, len);
	if (r == 0)
		panic("play failed");
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
	debug("enter n=%d", len);

	if (ao->is_playing)
		panic("playing not end");

	ao->play_buf = buf;
	ao->play_len = len;
	ao->on_play_done = done;

	ao->is_playing = 1;

	static uv_work_t w;
	w.data = ao;
	uv_queue_work(ao->loop, &w, play_thread, play_done);
	
	debug("leave n=%d", len);
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

