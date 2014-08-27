
#include <stdio.h>  
#include <linux/input.h>  
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

