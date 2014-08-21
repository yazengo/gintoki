
#include <sys/soundcard.h>
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libavdevice/avdevice.h>
#include <libavresample/avresample.h>
#include <libavutil/mathematics.h>

#include <utils.h>
#include <audio_out.h>

typedef struct {
	AVOutputFormat *ofmt;
	AVFormatContext *fmt_ctx;
	AVCodec *codec;
	AVStream *st;

	int fd_oss;

	uv_async_t async_main;
	uv_async_t async_audio;
	uv_loop_t *loop_audio;

	void *buf;
	int size;
	void (*cb)(void *);
	void *cb_p;
} ao_t;

static ao_t _ao, *ao = &_ao;

static void ao_play_oss(void *buf, int size) {
	log("size %d", size);
	write(ao->fd_oss, buf, size);
}

static void ao_play_libav_alsa(void *buf, int size) {
	AVPacket pkt = {};

	pkt.data = buf;
	pkt.size = size;

	log("size %d", size);
	ao->ofmt->write_packet(ao->fmt_ctx, &pkt);
}

void ao_play(void *buf, int size) {
#ifdef __mips__
	ao_play_oss(buf, size);
#else
	ao_play_libav_alsa(buf, size);
#endif
}

void ao_init_oss(int sample_rate) {
	int v, r;

	if (ao->fd_oss)
		close(ao->fd_oss);

	ao->fd_oss = open("/dev/dsp", O_WRONLY);
	if (ao->fd_oss < 0) {
		log("open dev failed: %s", strerror(errno));
		exit(-1);
	}

	v = AFMT_S16_LE;
	r = ioctl(ao->fd_oss, SNDCTL_DSP_SETFMT, &v);
	if (r < 0) {
		log("ioctl setfmt failed: %s", strerror(errno));
		exit(-1);
	}

	v = 2;
	r = ioctl(ao->fd_oss, SNDCTL_DSP_CHANNELS, &v);
	if (r < 0) {
		log("ioctl set channels failed: %s", strerror(errno));
		exit(-1);
	}

	v = sample_rate;
	r = ioctl(ao->fd_oss, SNDCTL_DSP_SPEED, &v);
	if (r < 0) {
		log("ioctl set speed failed: %s", strerror(errno));
		exit(-1);
	}

	if (v != sample_rate) {
		log("driver sample_rate changes: orig=%d ret=%d", sample_rate, v);
		exit(-1);
	}
}

static void ao_init_libav_alsa(int sample_rate) {
	if (ao->fmt_ctx == NULL) {
		av_register_all();
		avdevice_register_all();
		av_log_set_level(AV_LOG_DEBUG);

		ao->fmt_ctx = avformat_alloc_context();

		const char *drv = "alsa";

		log("open %s samplerate=%d", drv, sample_rate);

		ao->ofmt = av_guess_format(drv, NULL, NULL);

		if (ao->ofmt == NULL) {
			log("ofmt null");
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
	log("init: %d", r);
	if (r)
		exit(r);
}

void audio_out_play(void *buf, int size, void (*cb)(void *), void *cb_p) {
	ao->buf = buf;
	ao->size = size;
	ao->cb = cb;
	ao->cb_p = cb_p;
	uv_async_send(&ao->async_audio);
}

static void audio_thread(void *_) {
	uv_run(ao->loop_audio, UV_RUN_DEFAULT);
}

static void play_start(uv_async_t *a, int r) {
	ao_play(ao->buf, ao->size);
	uv_async_send(&ao->async_main);
}

static void play_done(uv_async_t *a, int r) {
	ao->cb(ao->cb_p);
}

void audio_out_init(uv_loop_t *loop_main, int sample_rate) {
  ao->loop_audio = uv_loop_new();

	uv_async_init(loop_main, &ao->async_main, play_done);
	uv_async_init(ao->loop_audio, &ao->async_audio, play_start);

	uv_thread_t tid;
	uv_thread_create(&tid, audio_thread, NULL);

#ifdef __mips__
	ao_init_oss(sample_rate);
#else
	ao_init_libav_alsa(sample_rate);
#endif
}

