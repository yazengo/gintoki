
#include <stdio.h>  
#include <linux/input.h>  
#include <sys/ioctl.h>
#include <fcntl.h>
#include <lua.h>
#include <uv.h>

#include "utils.h"

static int fd_gpio;
static int fd_vol;
static uv_loop_t *loop;
static lua_State *L;

static void dev_open(char *devname) {
	int fd = open(devname, O_RDWR);
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

	close(fd);
}

enum {
	KEYPRESS = 33,
};

// in main thread
static void call_event_done(void *pcall, void *_p) {
	int e = *(int *)_p;

	info("e=%d", e);

	lua_getglobal(L, "on_inputevent");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		return;
	}
	lua_pushnumber(L, e);
	lua_call_or_die(L, 1, 0);

	pthread_call_uv_complete(pcall);
}

// in poll thread
static void call_event(int e) {
	pthread_call_uv_wait_withname(loop, call_event_done, &e, "vol");
}

static void poll_gpio_thread(void *_) {
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
			if (last_e.value == 0) {
				// key down
				switch (stat) {
				case KEYDOWN: break;
				case NONE: stat = KEYDOWN; break;
				}
			} else {
				// key up
				switch (stat) {
				case KEYDOWN: stat = NONE; call_event(KEYPRESS); break;
				case NONE: break;
				}
			}
		}
		last_e = e;
	}
}

static void poll_vol_thread(void *_) {
	struct input_event e, last_e;

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
}

void inputdev_init(lua_State *_L, uv_loop_t *_loop) {

	for (;;) {
		int i;
		char name[256];
		for (i = 0; i < 8; i++) {
			sprintf(name, "/dev/input/event%d", i);
			dev_open(name);
		}
		if (fd_gpio == 0 || fd_vol == 0) {
			info("unable to get fds, retry in 1s");
			usleep(1e6 * 0.5);
			close(fd_gpio);
			close(fd_vol);
		} else
			break;
	}

	loop = _loop;
	L = _L;

	pthread_t tid;
	pthread_create(&tid, NULL, poll_gpio_thread, NULL);
	pthread_create(&tid, NULL, poll_vol_thread, NULL);
}

