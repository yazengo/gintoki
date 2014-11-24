
#include <ao/ao.h>

#include "aout.h"
#include "utils.h"

static void libao_list_drivers() {
	int n = 0, i;
	ao_info **d = ao_driver_info_list(&n);

	info("avail drvs:");
	for (i = 0; i < n; i++) {
		info("%s: %s", d[i]->short_name, d[i]->name);
	}
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

void *aoutdev_new() {
	ao_sample_format fmt = {};
	fmt.bits = 16;
	fmt.channels = 2;
	fmt.rate = 44100;
	fmt.byte_format = AO_FMT_LITTLE;

	int drv = ao_default_driver_id();
	if (drv == -1) {
		libao_list_drivers();
		panic("default driver id not found");
	}

	ao_device *dev = ao_open_live(drv, &fmt, NULL);
	if (dev == NULL)
		panic("open failed: %s", libao_strerror(errno));

	info("libao opened");

	return dev;
}

void aoutdev_close(void *_dev) {
	ao_device *dev = (ao_device *)_dev;
	ao_close(dev);
}

