
#include "utils.h"
#include "audio_out.h"

static void play_oss(audio_out_t *ao, void *buf, int size) {
	debug("size %d", size);
	write(ao->fd_oss, buf, size);
}

static void play_libav_alsa(audio_out_t *ao, void *buf, int size) {
	AVPacket pkt = {};

	pkt.data = buf;
	pkt.size = size;

	info("size %d", size);
	ao->ofmt->write_packet(ao->fmt_ctx, &pkt);
}

static void play(audio_out_t *ao, void *buf, int len) {
#ifdef __mips__
	play_oss(ao, buf, len);
#else
	play_libav_alsa(ao, buf, len);
#endif
}

static void init_oss(audio_out_t *ao, int sample_rate) {
	int v, r;

	if (ao->fd_oss)
		close(ao->fd_oss);

	ao->fd_oss = open("/dev/dsp", O_WRONLY);
	if (ao->fd_oss < 0) {
		debug("open dev failed: %s", strerror(errno));
		exit(-1);
	}

	v = AFMT_S16_LE;
	r = ioctl(ao->fd_oss, SNDCTL_DSP_SETFMT, &v);
	if (r < 0) {
		debug("ioctl setfmt failed: %s", strerror(errno));
		exit(-1);
	}

	v = 2;
	r = ioctl(ao->fd_oss, SNDCTL_DSP_CHANNELS, &v);
	if (r < 0) {
		debug("ioctl set channels failed: %s", strerror(errno));
		exit(-1);
	}

	v = sample_rate;
	r = ioctl(ao->fd_oss, SNDCTL_DSP_SPEED, &v);
	if (r < 0) {
		debug("ioctl set speed failed: %s", strerror(errno));
		exit(-1);
	}

	if (v != sample_rate) {
		debug("driver sample_rate changes: orig=%d ret=%d", sample_rate, v);
		exit(-1);
	}
}

static void init_libav_alsa(audio_out_t *ao, int sample_rate) {

	if (ao->fmt_ctx == NULL) {
		av_register_all();
		avdevice_register_all();
		av_log_set_level(AV_LOG_DEBUG);

		ao->fmt_ctx = avformat_alloc_context();

		const char *drv = "alsa";

		debug("open %s samplerate=%d", drv, sample_rate);

		ao->ofmt = av_guess_format(drv, NULL, NULL);

		if (ao->ofmt == NULL) {
			debug("ofmt null");
			exit(-1);
		}
	}

	if (ao->codec == NULL) {
		ao->codec = avcodec_find_encoder(AV_CODEC_ID_MP3);
		ao->st = avformat_new_stream(ao->fmt_ctx, ao->codec);
		ao->fmt_ctx->priv_data = av_mallocz(ao->ofmt->priv_data_size);
	}

	ao->st->codec->sample_rate = sample_rate;
	ao->st->codec->channels = 2;

	int r = ao->ofmt->write_header(ao->fmt_ctx);
	info("init: %d", r);
	if (r) {
		error("init failed");
		exit(-1);
	}
}

// on thread main
static void play_done(uv_work_t *w, int stat) {
	audio_out_t *ao = (audio_out_t *)w->data;

	ao->play_buf = NULL;
	ao->on_play_done(ao);
	free(w);
}

// on thread play
static void play_thread(uv_work_t *w) {
	audio_out_t *ao = (audio_out_t *)w->data;
	play(ao, ao->play_buf, ao->play_len);
}

// on thread main
void audio_out_play(audio_out_t *ao, void *buf, int len, void (*done)(audio_out_t *)) {
	if (ao->play_buf)
		return;

	ao->play_buf = buf;
	ao->play_len = len;
	ao->on_play_done = done;

	uv_work_t *w = (uv_work_t *)zalloc(sizeof(uv_work_t));
	w->data = ao;
	uv_queue_work(ao->loop, w, play_thread, play_done);
}

void audio_out_init(uv_loop_t *loop, audio_out_t *ao, int sample_rate) {
	ao->loop = loop;

#ifdef __mips__
	init_oss(ao, sample_rate);
#else
	init_libav_alsa(ao, sample_rate);
#endif
}

