
#include <unistd.h>
#include <errno.h>
#include <sys/soundcard.h>

#include "aout.h"
#include "utils.h"

typedef struct {
	int fd;
} jzcodec_t;

void *aoutdev_new() {
	jzcodec_t *jz = (jzcodec_t *)zalloc(sizeof(jzcodec_t));
	int v, r;
	int rate = 44100;

	info("init");

	jz->fd = open("/dev/dsp", O_WRONLY);
	if (jz->fd < 0) 
		panic("open dev failed: %s", strerror(errno));

	v = AFMT_S16_LE;
	r = ioctl(jz->fd, SNDCTL_DSP_SETFMT, &v);
	if (r < 0) 
		panic("ioctl setfmt failed: %s", strerror(errno));

	v = 12|(8<<16);
	r = ioctl(jz->fd, SNDCTL_DSP_SETFRAGMENT, &v);
	if (r < 0) 
		panic("ioctl setfragment: %s", strerror(errno));

	v = 2;
	r = ioctl(jz->fd, SNDCTL_DSP_CHANNELS, &v);
	if (r < 0) 
		panic("ioctl set channels failed: %s", strerror(errno));

	v = rate;
	r = ioctl(jz->fd, SNDCTL_DSP_SPEED, &v);
	if (r < 0) 
		panic("ioctl set speed failed: %s", strerror(errno));

	if (v != rate) 
		panic("driver sample_rate changes: orig=%d ret=%d", rate, v);

	info("done");
}

void aoutdev_close(void *_dev) {
	jzcodec_t *jz = (jzcodec_t *)_dev;
	close(jz->fd);
}

