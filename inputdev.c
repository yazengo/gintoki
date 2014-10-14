
#include <stdlib.h>  
#include <stdio.h>  
#include <linux/input.h>  
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <fcntl.h>
#include <lua.h>
#include <uv.h>

#include "utils.h"

typedef struct inotify_s {
	int fd;
	uv_fs_t *req;
	uv_pipe_t *pipe;
	char buf[512];
	void *data;
	void (*on_create)(struct inotify_s *in, char *name);
	void (*on_delete)(struct inotify_s *in, char *name);
} fsevent_t;

static uv_buf_t fsevent_allocbuf(uv_handle_t *h, size_t len) {
	fsevent_t *in = h->data;
	return uv_buf_init(in->buf, sizeof(in->buf));
}

static void fsevent_read(uv_stream_t *h, ssize_t nread, uv_buf_t buf) {
	fsevent_t *in = h->data;

	debug("read=%d", nread);
	if (nread <= sizeof(struct inotify_event))
		return;

	struct inotify_event *ie = (struct inotify_event *)buf.base;
	if (ie->mask & IN_CREATE) {
		debug("create name=%s", ie->name);
		if (in->on_create)
			in->on_create(in, ie->name);
	}
	if (ie->mask & IN_DELETE) {
		debug("delete name=%s", ie->name);
		if (in->on_delete)
			in->on_delete(in, ie->name);
	}
}

static void fsevent_init(uv_loop_t *loop, fsevent_t *in, char *path) {
	in->fd = inotify_init();

	debug("fd=%d", in->fd);

	int r = inotify_add_watch(in->fd, path, IN_DELETE|IN_CREATE);
	if (r < 0)
		panic("add watch failed");

	in->req = zalloc(sizeof(uv_fs_t));
	in->req->data = in;

	in->pipe = zalloc(sizeof(uv_pipe_t));
	in->pipe->data = in;
	uv_pipe_init(loop, in->pipe, 0);
	uv_pipe_open(in->pipe, in->fd);

	uv_read_start((uv_stream_t *)in->pipe, fsevent_allocbuf, fsevent_read);
}

enum {
	KEYPRESS = 33,
	KEYDBLCLICK = 331,
	KEYLONGPRESS = 332,

	SLEEP = 34,
	WAKEUP = 35,
	NETWORK_UP = 36,
	NETWORK_DOWN = 37,

	VOLEND = 38,

	PREV = 40,
	NEXT = 41,
};

enum {
	GPIOKEYS, SUGRVOL, NETNOTIFY, GSENSOR, LIS3DH,
	INPUTDEV_NR,
};

static const char *inputdev_names[] = {
	"gpio-keys", "Sugr_Volume", "network_notify", "gsensor_dev", "lis3dh_acc",
};

typedef struct {
	lua_State *L;
	uv_loop_t *loop;
	fsevent_t *fsevent;

	int gpiokeys_stat;
	int gpiokeys_stat_dblclk;
	float gpiokeys_presstm;
	uv_timer_t *gpiokeys_timer;

	int lis3dh_verbose:1;
	FILE *lis3dh_logfp;
	int lis3dh_xyz[3];

	int fds[INPUTDEV_NR];
	struct input_event ev[INPUTDEV_NR];
	struct input_event last_ev[INPUTDEV_NR];

	uv_work_t *work;
	int pollidx[INPUTDEV_NR];
	struct pollfd pollfds[INPUTDEV_NR+1];
	int pollpipe[2];
	int pollnr;
	int polling;

} inputdev_t;

static inputdev_t _dev, *dev = &_dev;

static void inputdev_emit_event(int e) {
	lua_State *L = dev->L;

	lua_getglobal(L, "inputdev_on_event");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return;
	}
	lua_pushnumber(L, e);
	lua_call_or_die(L, 1, 0);
}

enum { NONE, KEYDOWN, DBL1 }; // gpiokeys_stat

static void gpiokeys_timer_free(uv_handle_t *t) {
	free(t);
}

static void gpiokeys_init() {
	dev->gpiokeys_stat = NONE;
}

static void gpiokeys_timeout(uv_timer_t *t, int _) {
	uv_close((uv_handle_t *)dev->gpiokeys_timer, gpiokeys_timer_free);
	dev->gpiokeys_timer = NULL;

	debug("single press");
	inputdev_emit_event(KEYPRESS); 
	dev->gpiokeys_stat_dblclk = NONE;
}

static void gpiokeys_onkey() {
	debug("stat=%d", dev->gpiokeys_stat_dblclk);

	switch (dev->gpiokeys_stat_dblclk) {
	case NONE:
		dev->gpiokeys_timer = (uv_timer_t *)zalloc(sizeof(uv_timer_t));
		uv_timer_init(dev->loop, dev->gpiokeys_timer);
		uv_timer_start(dev->gpiokeys_timer, gpiokeys_timeout, 300, 0);
		dev->gpiokeys_stat_dblclk = DBL1;
		break;

	case DBL1:
		uv_timer_stop(dev->gpiokeys_timer);
		uv_close((uv_handle_t *)dev->gpiokeys_timer, gpiokeys_timer_free);
		dev->gpiokeys_timer = NULL;
		debug("dbl click");
		inputdev_emit_event(KEYDBLCLICK); 
		dev->gpiokeys_stat_dblclk = NONE;
		break;
	}
}

static void gpiokeys_read(struct input_event e) {
	int stat = dev->gpiokeys_stat;
	struct input_event last_e = dev->last_ev[GPIOKEYS];

	if (e.type == EV_SYN && last_e.type != EV_SYN) {
		if (last_e.code == 0x8e) {
			inputdev_emit_event(VOLEND); 
		}

		if (last_e.code == 0xa4) {

			if (last_e.value == 1) {
				// key down
				if (stat == NONE) {
					stat = KEYDOWN;
				}
			} else {
				// key up
				if (stat == KEYDOWN) {
					stat = NONE;
					inputdev_emit_event(KEYPRESS);
				}
			}
		}

	}

	dev->gpiokeys_stat = stat;
	dev->last_ev[GPIOKEYS] = e;
}

static void sugrvol_read(struct input_event e) {
	struct input_event last_e = dev->last_ev[SUGRVOL];

	if (e.type == EV_SYN && last_e.type != EV_SYN) {
		inputdev_emit_event(last_e.value);
	}

	dev->last_ev[SUGRVOL] = e;
}

static void netnotify_read(struct input_event e) {
	struct input_event last_e = dev->last_ev[NETNOTIFY];

	if (e.type == EV_SYN && last_e.type != EV_SYN) {
		if (last_e.code == 150)
			inputdev_emit_event(last_e.value ? NETWORK_UP : NETWORK_DOWN);
	}

	dev->last_ev[NETNOTIFY] = e;
}

static void gsensor_read(struct input_event e) {
	struct input_event last_e = dev->last_ev[GSENSOR];

	if (e.type == EV_SYN && last_e.type != EV_SYN) {
		if (last_e.value == 1) {
			if (last_e.code == KEY_BACK) 
				inputdev_emit_event(PREV);
			if (last_e.code == KEY_FORWARD)
				inputdev_emit_event(NEXT);
		}
	}

	dev->last_ev[GSENSOR] = e;
}

static void lis3dh_init() {
	char *s;

	dev->lis3dh_verbose = (getenv("LIS3DH_VERBOSE") != NULL);

	s = getenv("LIS3DH_LOG");
	if (s) {
		dev->lis3dh_logfp = fopen(s, "w+");
	}

	int rate = 10;
	s = getenv("LIS3DH_RATE");
	if (s) {
		sscanf(s, "%d", &rate);

		FILE *fp = fopen("/sys/class/i2c-adapter/i2c-0/0-0018/pollrate_ms", "w");
		if (fp) {
			fprintf(fp, "%d", rate);
			fclose(fp);
		}
		info("rate=%dms", rate);
	}
}

static void lis3dh_read(struct input_event e) {
	if (e.type == EV_SYN) {
		int tm = e.time.tv_sec*1000000 + e.time.tv_usec;
		int *x = dev->lis3dh_xyz;
		if (dev->lis3dh_verbose)
			info("tm=%d xyz=%d,%d,%d", tm, x[0], x[1], x[2]);
		if (dev->lis3dh_logfp)
			fprintf(dev->lis3dh_logfp, "%d,%d,%d,%d,\n", tm, x[0], x[1], x[2]);
	} else {
		if (e.code <= 2)
			dev->lis3dh_xyz[e.code] = e.value;
	}
}

typedef void (*inputdev_init_cb)();
typedef void (*inputdev_read_cb)(struct input_event e);

static inputdev_init_cb inputdev_initcbs[] = {
	gpiokeys_init, NULL, NULL, NULL, lis3dh_init,
};

static inputdev_read_cb inputdev_readcbs[] = {
	gpiokeys_read, sugrvol_read, netnotify_read, gsensor_read, lis3dh_read,
};

static void inputdev_poll_start();

static void inputdev_poll_thread(uv_work_t *w) {
	debug("poll start nr=%d", dev->pollnr);
	
	int r = poll(dev->pollfds, dev->pollnr+1, -1);
	int i;
	for (i = 0; i < dev->pollnr; i++) {
		struct pollfd *f = &dev->pollfds[i];
		int fi = dev->pollidx[i];
		int fd = dev->fds[fi];

		debug("revents=%x name=%s", f->revents, inputdev_names[fi]);

		if (f->revents & (POLLERR|POLLHUP)) {
			close(fd);
		} else if (f->revents & POLLIN) {
			read(fd, &dev->ev[fi], sizeof(struct input_event));
		}
	}
	if (dev->pollfds[dev->pollnr].revents & POLLIN) {
		char ch;
		read(dev->pollpipe[0], &ch, 1);
	}
}

static void inputdev_poll_done(uv_work_t *w, int _) {
	int i;
	debug("queue work done");

	for (i = 0; i < dev->pollnr; i++) {
		struct pollfd *f = &dev->pollfds[i];
		int fi = dev->pollidx[i];
		if (f->revents & (POLLERR|POLLHUP)) {
			info("closed name=%s", inputdev_names[fi]);
			dev->fds[fi] = 0;
		} else if (f->revents & POLLIN) {
			debug("%s code=%d value=%d", inputdev_names[fi], dev->ev[fi].code, dev->ev[fi].value);
			inputdev_readcbs[fi](dev->ev[fi]);
		}
	}

	dev->polling = 0;

	inputdev_poll_start();
}

static void inputdev_poll_start() {
	if (dev->polling) {
		write(dev->pollpipe[1], "", 1);
		return;
	}
		
	int pi = 0;
	int i;

	for (i = 0; i < INPUTDEV_NR; i++) {
		if (dev->fds[i]) {
			dev->pollfds[pi].fd = dev->fds[i];
			dev->pollfds[pi].events = POLLIN|POLLERR|POLLHUP;
			dev->pollidx[pi] = i;
			pi++;
		}
	}
	dev->pollfds[pi].fd = dev->pollpipe[0];
	dev->pollfds[pi].events = POLLIN;
	dev->pollnr = pi;

	debug("queue work");
	uv_queue_work(dev->loop, dev->work, inputdev_poll_thread, inputdev_poll_done);
	dev->polling = 1;
}

static void inputdev_open(char *path) {
	int fd = open(path, O_RDONLY);
	if (fd == -1)
		return;

	char name[80];
	if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 1) {
		close(fd);
		return;
	}

	int i;
	for (i = 0; i < INPUTDEV_NR; i++) {
		if (!strcmp(name, inputdev_names[i]))
			break;
	}

	if (i == INPUTDEV_NR) {
		close(fd);
		return;
	}

	if (dev->fds[i]) {
		close(fd);
		return;
	}

	dev->fds[i] = fd;

	if (inputdev_initcbs[i])
		inputdev_initcbs[i]();

	int n = 0, j;
	for (j = 0; j < INPUTDEV_NR; j++)
		n += !!dev->fds[j];
	info("open name=%s n=%d i=%d", name, n, i);

	inputdev_poll_start();
}

static void inputdev_scan() {
	int i;
	for (i = 0; i < 10; i++) {
		char path[512];
		sprintf(path, "/dev/input/event%d", i);
		inputdev_open(path);
	}
}

static void inputdev_on_create(fsevent_t *e, char *name) {
	info("name=%s", name);
	char path[512];
	sprintf(path, "/dev/input/%s", name);
	inputdev_open(path);
}

static void lua_inputdev_init(lua_State *L) {
	uv_loop_t *loop = (uv_loop_t *)lua_touserptr(L, lua_upvalueindex(1));

	info("init");

	dev->L = L;
	dev->loop = loop;

	dev->work = (uv_work_t *)zalloc(sizeof(uv_work_t));

	pipe(dev->pollpipe);

	dev->fsevent = (fsevent_t *)zalloc(sizeof(fsevent_t));
	dev->fsevent->on_create = inputdev_on_create;
	fsevent_init(loop, dev->fsevent, "/dev/input");

	inputdev_scan();
}

void luv_inputdev_init(lua_State *L, uv_loop_t *loop) {
	lua_pushuserptr(L, loop);
	lua_pushcclosure(L, lua_inputdev_init, 1);
	lua_setglobal(L, "inputdev_init");
}

