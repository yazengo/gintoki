
#include <stdio.h>  
#include <linux/input.h>  
#include <sys/inotify.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <lua.h>
#include <uv.h>

#include "utils.h"

static int fd_gpio;
static int fd_vol;
static int fd_network_notify;
static int fd_gsensor;
static uv_loop_t *loop;
static lua_State *L;

static void dev_open(char *devname) {
	int fd = open(devname, O_RDONLY);
	if (fd == -1)
		return;

	char name[80];
	char location[80];
	char idstr[80];
	if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 1) {
		close(fd);
		return;
	}

	info("name=%s", name);
	if (!strcmp(name, "gpio-keys")) {
		fd_gpio = fd;
		info("name=%s gpio_fd=%d", name, fd);
		return;
	}

	if (!strcmp(name, "Sugr_Volume")) {
		fd_vol = fd;
		info("name=%s vol_fd=%d", name, fd);
		return;
	}

	if (!strcmp(name, "network_notify")) {
		fd_network_notify = fd;
		info("name=%s network_notify_fd=%d", name, fd);
		return;
	}

	if (!strcmp(name, "gsensor_dev")) {
		fd_gsensor = fd;
		info("name=%s fd_gsensor=%d", name, fd);
		return;
	}

	close(fd);
}

enum {
	KEYPRESS = 33,

	SLEEP = 34,
	WAKEUP = 35,
	NETWORK_UP = 36,
	NETWORK_DOWN = 37,

	VOLEND = 38,

	PREV = 40,
	NEXT = 41,
};

// in main thread
static void call_event_done(void *pcall, void *_p) {
	int e = *(int *)_p;
	pthread_call_uv_complete(pcall);

	info("e=%d", e);

	lua_getglobal(L, "on_inputevent");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return;
	}
	lua_pushnumber(L, e);
	lua_call_or_die(L, 1, 0);
}

// in poll thread
static void call_event(int e) {
	info("e=%d", e);
	pthread_call_uv_wait_withname(loop, call_event_done, &e, "inputdev");
}

static void *poll_gpio_thread(void *_) {
	struct input_event e = {}, last_e = {};
	enum {
		NONE, KEYDOWN,
	};
	int stat = NONE;

	// fd_gpio: EV_KEY
	for (;;) {
		int r = read(fd_gpio, &e, sizeof(e));
		if (r < 0)
			panic("gpio read failed");
		info("gpio_key: type=%x code=%x value=%x", e.type, e.code, e.value);
		if (e.type == EV_SYN && last_e.type != EV_SYN) {
			if (last_e.code == 0x8e) {
				call_event(VOLEND); 
			}
			if (last_e.code == 0xa4) {
				if (last_e.value == 0) {
					// key down
					switch (stat) {
					case KEYDOWN: break;
					case NONE: stat = KEYDOWN; break;
					}
				} else {
					// key up
					switch (stat) {
					case KEYDOWN: 
						stat = NONE; 
						call_event(KEYPRESS); 
						break;
					case NONE: break;
					}
				}
			}
		}
		last_e = e;
	}
	return NULL;
}

static void *poll_vol_thread(void *_) {
	struct input_event e = {}, last_e = {};

	// fd_vol: EV_ABS
	for (;;) {
		int r = read(fd_vol, &e, sizeof(e));
		info("vol: type=%d code=%d value=%d", e.type, e.code, e.value);
		if (r < 0)
			panic("vol read failed");
		if (e.type == EV_SYN && last_e.type != EV_SYN) {
			call_event(last_e.value);
		}
		last_e = e;
	}
	return NULL;
}

static void *poll_gsensor_thread(void *_) {
	struct input_event e = {}, last_e = {};
	int stat = KEY_PLAY;

	for (;;) {
		int r = read(fd_gsensor, &e, sizeof(e));
		info("gsensor: type=%d code=%d value=%d", e.type, e.code, e.value);
		if (r < 0)
			panic("gsensor read failed");
		if (e.type == EV_SYN && last_e.type != EV_SYN) {
			if (last_e.value == 1) {
				if (last_e.code == KEY_BACK) 
					call_event(PREV);
				if (last_e.code == KEY_FORWARD)
					call_event(NEXT);
			}
		}
		last_e = e;
	}
	return NULL;
}


static void *poll_network_notify_thread(void *_) {
	struct input_event e = {}, last_e = {};

	for (;;) {
		int r = read(fd_network_notify, &e, sizeof(e));
		info("type=%d code=%d value=%d", e.type, e.code, e.value);
		if (r < 0)
			panic("read failed");
		if (e.type == EV_SYN && last_e.type != EV_SYN) {
			if (last_e.code == 150)
				call_event(last_e.value ? NETWORK_UP : NETWORK_DOWN);
		}
		last_e = e;
	}
	return NULL;
}

void inputdev_init(lua_State *_L, uv_loop_t *_loop) {

	for (;;) {
		int i;
		char name[256];
		for (i = 0; i < 8; i++) {
			sprintf(name, "/dev/input/event%d", i);
			dev_open(name);
		}
		if (!fd_gpio || !fd_vol || !fd_network_notify || !fd_gsensor) {
			info("unable to get all fds, retry in 1s");
			close(fd_gpio);
			close(fd_vol);
			close(fd_gsensor);
			close(fd_network_notify);
			sleep(1);
		} else
			break;
	}

	loop = _loop;
	L = _L;

	pthread_t tid;
	pthread_create(&tid, NULL, poll_gpio_thread, NULL);
	pthread_create(&tid, NULL, poll_vol_thread, NULL);
	pthread_create(&tid, NULL, poll_network_notify_thread, NULL);
	pthread_create(&tid, NULL, poll_gsensor_thread, NULL);
}

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
	uv_pipe_init(in->loop, in->pipe, 0);
	uv_pipe_open(in->pipe, in->fd);

	uv_read_start((uv_stream_t *)in->pipe, fsevent_allocbuf, fsevent_read);
}

enum {
	GPIO, VOL, NETWORK, GSENSOR,
	INPUTDEV_NR,
};

static const char *inputdev_names[] = {
	"gpio-keys", "Sugr_Volume", "network_notify", "gesensor_dev",
};

typedef struct {
	lua_State *L;
	uv_loop_t *loop;
	fsevent_t *fsevent;

	uv_pipe_t *pipes[INPUTDEV_NR];
	char buf[INPUTDEV_NR][sizeof(struct input_event)];
} inputdev_t;

static uv_buf_t inputdev_allocbuf(uv_handle_t *h, size_t len) {
	int i = (int)h->data;
	return uv_buf_init(dev->buf[i], sizeof(dev->buf[i]));
}

static void gpiokeys_read(uv_stream_t *h, ssize_t nread, uv_buf_t buf) {
}

static void sugrvol_read(uv_stream_t *h, ssize_t nread, uv_buf_t buf) {
}

static void netnotify_read(uv_stream_t *h, ssize_t nread, uv_buf_t buf) {
}

static void gsensor_read(uv_stream_t *h, ssize_t nread, uv_buf_t buf) {
}

static uv_read_cb inputdev_readcbs[] = {
	gpiokeys_read, sugrvol_read, netnotify_read, gsensor_read,
};

static inputdev_t _dev, *dev = &_dev;

static void inputdev_open(char *path) {
	if (dev->pipes[i])
		return;

	int fd = open(path, O_RDONLY);
	if (fd == -1)
		return;

	char name[80];
	char location[80];
	char idstr[80];
	if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 1) {
		close(fd);
		return;
	}

	debug("name=%s", name);

	int i;
	for (i = 0; i < INPUTDEV_NR; i++) {
		if (!strcmp(name, inputdev_names[i]))
			break;
	}

	if (i == INPUTDEV_NR) {
		close(fd);
		return;
	}

	dev->pipes[i] = zalloc(sizeof(uv_pipe_t));
	dev->pipes[i]->data = (void *)i;
	uv_pipe_init(dev->loop, dev->pipes[i], 0);
	uv_pipe_open(dev->pipes[i], fd);
	uv_read_start((uv_stream_t *)in->pipes[i], inputdev_allocbuf, inputdev_readcbs[i]);
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

void luv_inputdev_init(lua_State *L, uv_loop_t *loop) {
	int r;

	dev->L = L;
	dev->loop = loop;

	dev->fsevent = zalloc(sizeof(fsevent_t));
	dev->fsevent->on_create = inputdev_on_create;
	fsevent_init(dev->fsevent, "/dev/input");

	inputdev_scan();
}

