
#include <sys/ioctl.h>
#include <lua.h>
#include <uv.h>

#include "utils.h"

static void dev_open(char *devname) {
	int fd = open(devname, O_RDWR);
	if (fd == -1)
		return;

	char name[80];
	char location[80];
	char idstr[80];
	if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 1)
		return;

	info("name=%s", name);
}

void inputdev_init(lua_State *L, uv_loop_t *loop) {
	int i;
	char name[256];

	for (i = 0; i < 8; i++) {
		sprintf(name, "/dev/input/event%d", i);
		dev_open(name);
	}
}

