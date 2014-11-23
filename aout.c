
#include <ao/ao.h>

#include "utils.h"
#include "luv.h"
#include "pipe.h"

typedef struct {
	ao_device *dev;
	uv_work_t w;
	uv_buf_t ub;
} aout_t;

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

static void deinit(uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;
	aout_t *ao = (aout_t *)p->data;

	ao_close(ao->dev);
	free(ao);
}

static void init(aout_t *ao) {
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

	ao->dev = ao_open_live(drv, &fmt, NULL);
	if (ao->dev == NULL)
		panic("open failed: %s", libao_strerror(errno));

	info("libao opened");
}

static void read_done(pipe_t *p, ssize_t n, uv_buf_t ub);

static void play_thread(uv_work_t *w) {
	pipe_t *p = (pipe_t *)w->data;
	aout_t *ao = (aout_t *)p->data;

	debug("n=%d", ao->ub.len);
	ao_play(ao->dev, ao->ub.base, ao->ub.len);
}

static void play_done(uv_work_t *w, int stat) {
	pipe_t *p = (pipe_t *)w->data;

	pipe_read(p, NULL, read_done);
}

static void read_done(pipe_t *p, ssize_t n, uv_buf_t ub) {
	debug("n=%d", n);

	if (n < 0) {
		pipe_close_read(p);
		return;
	}

	aout_t *ao = (aout_t *)p->data;
	ao->ub = ub;
	ao->w.data = p;
	uv_queue_work(luv_loop(p), &ao->w, play_thread, play_done);
}

int luv_aout(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	aout_t *ao = (aout_t *)zalloc(sizeof(aout_t));

	p->data = ao;
	p->type = PDIRECT_SINK;

	init(ao);
	luv_setgc(p, deinit);

	pipe_read(p, NULL, read_done);

	return 1;
}

void luv_aout_init(lua_State *L, uv_loop_t *loop) {
	ao_initialize();

	luv_register(L, loop, "aout", luv_aout);
}

