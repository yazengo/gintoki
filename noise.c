
#include <stdlib.h>
#include <math.h>

#include "luv.h"
#include "utils.h"

#include "audio_in.h"

typedef struct {
	uv_loop_t *loop;
	int type;
	audio_in_read_cb read;
	audio_in_close_cb close;
	audio_in_t *ai;
	int len;
	uv_call_t c;
} cmd_t;

typedef struct {
	unsigned stopped:1;
	uv_loop_t *loop;
} noise_t;

enum {
	READ,
	CLOSE,
};

static void on_cmd(uv_call_t *h) {
	cmd_t *c = (cmd_t *)h->data;
	audio_in_t *ai = c->ai;
	noise_t *n = (noise_t *)ai->in;

	switch (c->type) {
	case READ:
		c->read(ai, c->len);
		break;

	case CLOSE:
		c->close(ai);
		free(n);
		break;
	}

	free(c);
}

static char pink_noise[] = {
	0x7a, 0x00, 0x2a, 0x00, 0x5f, 0x00, 0xf9, 0xff, 0x92, 0xfe, 0xb1, 0x00,
	0x8d, 0x02, 0xac, 0xff, 0x62, 0xff, 0x78, 0x02, 0x29, 0x01, 0x20, 0xfe,
	0x38, 0xfe, 0xf8, 0xfe, 0x91, 0xff, 0xa4, 0xff, 0x10, 0xff, 0xf6, 0x00,
	0x7a, 0x02, 0x39, 0x00, 0x39, 0x00, 0xaf, 0x02, 0xfd, 0x00, 0xed, 0xfe,
	0xc9, 0x00, 0x48, 0x01, 0x14, 0xff, 0x10, 0xfe, 0xe7, 0xfe, 0x6a, 0x00,
	0x9c, 0x00, 0x98, 0xff, 0x35, 0x00, 0x89, 0x00, 0x91, 0xfe, 0xc4, 0xfe,
	0xe6, 0x00, 0x31, 0x00, 0x2f, 0xff, 0x51, 0x00, 0xbf, 0xff, 0xce, 0xfd,
	0x12, 0xff, 0x7c, 0x02, 0xa0, 0x02, 0xa6, 0xfe, 0x56, 0xfc, 0x70, 0xfe,
	0xf1, 0xff, 0x4e, 0xfe, 0x36, 0xfd, 0x5f, 0xfe, 0xaf, 0xff, 0x17, 0xff,
	0xd3, 0xfd, 0x16, 0xff, 0xa4, 0x00, 0xe0, 0xff, 0xd3, 0x01, 0x4f, 0x05,
	0xd6, 0x02, 0x04, 0x00, 0x4d, 0x02, 0x04, 0x02, 0x86, 0x00, 0x9c, 0x03,
	0x0d, 0x03, 0x3a, 0xfe, 0x43, 0xff, 0xed, 0x01, 0x96, 0x00, 0xc3, 0x00,
	0x36, 0x01, 0xfd, 0xfe, 0x00, 0xff, 0x95, 0x00, 0xdd, 0x00, 0x6d, 0x02,
	0xf4, 0x03, 0x31, 0x03, 0x97, 0x01, 0xa5, 0xfe, 0xb7, 0xfc, 0x57, 0xfe,
	0x58, 0xfe, 0x03, 0xfd, 0x8b, 0xff, 0xe0, 0x00, 0x4a, 0xfe, 0x04, 0xfe,
	0x1a, 0xff, 0x1b, 0xff, 0x76, 0x01, 0x2f, 0x03, 0xfa, 0xff, 0x95, 0xfc,
	0x19, 0xfd, 0x25, 0x00, 0xcd, 0x02, 0xf8, 0x02, 0xf6, 0x01, 0xe0, 0x00,
	0x2c, 0x00, 0xcf, 0x02, 0x3d, 0x05, 0x3c, 0x02, 0xba, 0x00, 0x62, 0x01,
	0xc3, 0xfa, 0xf1, 0xf5, 0x2b, 0xfe, 0x52, 0x05, 0xea, 0x01, 0x1b, 0xfe,
	0x15, 0xfe, 0x92, 0xff, 0xa4, 0x02, 0x14, 0x04, 0xc7, 0x01, 0x49, 0xfe,
	0x81, 0xfd, 0xe9, 0x00, 0x07, 0x01, 0x19, 0xfb, 0x74, 0xfa, 0x9c, 0xfe,
	0x95, 0xfe, 0x45, 0x00, 0x56, 0x03, 0x9d, 0x01, 0x7c, 0x03, 0x44, 0x02,
	0xec, 0xf5, 0x04, 0xfb, 0x30, 0x12, 0x6a, 0x13, 0xd6, 0x07, 0xc6, 0x08,
	0x6e, 0x03, 0xb4, 0xfa, 0xfa, 0xfe, 0x17, 0xf7, 0x8d, 0xe4, 0xa4, 0xe7,
	0x82, 0xf1, 0x42, 0xf1, 0xe5, 0xf4, 0x56, 0xf5, 0xc4, 0xf0, 0x1f, 0xf7,
	0x71, 0xfb, 0xb3, 0xfb, 0xcf, 0x0b, 0xd9, 0x14, 0xd0, 0x02, 0xc0, 0xf4,
	0x0a, 0xf7, 0x8b, 0xf9, 0xf4, 0xfb, 0xc8, 0x00, 0x3b, 0x08, 0x6e, 0x0d,
	0xe1, 0x02, 0xce, 0xf8, 0x35, 0x00, 0x50, 0xfc, 0x20, 0xee, 0x5f, 0xf9,
	0x58, 0x06, 0xf6, 0xf9, 0xc4, 0xf2, 0xe7, 0xf4, 0xd5, 0xf0, 0x79, 0xfa,
	0xe1, 0x0b, 0xa0, 0x09, 0x91, 0x02, 0xdc, 0x01, 0xc3, 0xfb, 0xe7, 0xf7,
	0xc7, 0xf7, 0x5a, 0xf3, 0xd2, 0xf4, 0x79, 0xf5, 0x2e, 0xe8, 0x3e, 0xe2,
	0x1c, 0xe9, 0x6a, 0xe8, 0x7b, 0xe8, 0xef, 0xef, 0x13, 0xf1, 0xe3, 0xf6,
	0x94, 0x03, 0xe6, 0xfb, 0xc4, 0xef, 0x6c, 0xff, 0x51, 0x08, 0xfb, 0xed,
	0x89, 0xdc, 0xf6, 0xe4, 0xaf, 0xe1, 0x53, 0xde, 0x4b, 0xf8, 0xde, 0x05,
	0xf4, 0xf4, 0xe2, 0xfd, 0x95, 0x18, 0xfa, 0x06, 0x36, 0xe4, 0xcb, 0xe3,
	0x87, 0xf1, 0xe9, 0xfc, 0x73, 0x07, 0x40, 0xff, 0xa5, 0xee, 0x32, 0xe8,
	0x53, 0xdd, 0xdf, 0xd5, 0xae, 0xdf, 0x5e, 0xe8, 0xd2, 0xf3, 0xc8, 0xfe,
	0x51, 0xeb, 0xc4, 0xe0, 0x17, 0xff, 0xe2, 0xfc, 0xde, 0xd1, 0xc6, 0xd1,
	0x14, 0xeb, 0x0f, 0xe2, 0x8b, 0xd1, 0xc6, 0xd1, 0x5c, 0xd7, 0x97, 0xdc,
	0x7b, 0xd3, 0xd1, 0xca, 0xd3, 0xdc, 0xfa, 0xe7, 0x62, 0xdd, 0xbe, 0xda,
	0xe2, 0xd1, 0xc3, 0xc8, 0xf5, 0xe6, 0xde, 0xfb, 0x5c, 0xe2, 0x6e, 0xdf,
};

enum {
	PINK_NOISE, WHITE_NOISE,
};
static int noise_mode = WHITE_NOISE;

static void gen(void *buf, int len) {
	switch (noise_mode) {
	case PINK_NOISE:
		while (len > 0) {
			int n = len;
			if (n > sizeof(pink_noise))
				n = sizeof(pink_noise);
			memcpy(buf, pink_noise, n);
			len -= n;
		}
		break;

	case WHITE_NOISE:
		while (len > 0) {
			*(uint16_t *)buf = rand()&0x7fff;
			buf += 2;
			len -= 2;
		}
		break;
	}
}

static void audio_in_read(audio_in_t *ai, void *buf, int len, audio_in_read_cb done) {
	noise_t *n = (noise_t *)ai->in;

	cmd_t *c = (cmd_t *)zalloc(sizeof(cmd_t));
	c->ai = ai;
	c->read = done;
	c->type = READ;
	c->c.done_cb = on_cmd;
	c->c.data = c;

	if (!n->stopped) {
		gen(buf, len);
		c->len = len;
	} else {
		c->len = -1;
	}

	uv_call(n->loop, &c->c);
}

static void audio_in_stop_read(audio_in_t *ai) {
	noise_t *n = (noise_t *)ai->in;

	n->stopped = 1;
}

static void audio_in_close(audio_in_t *ai, audio_in_close_cb done) {
	noise_t *n = (noise_t *)ai->in;

	cmd_t *c = (cmd_t *)zalloc(sizeof(cmd_t));
	c->ai = ai;
	c->close = done;
	c->type = CLOSE;
	c->c.done_cb = on_cmd;
	c->c.data = c;

	uv_call(n->loop, &c->c);
}

void audio_in_noise_init(uv_loop_t *loop, audio_in_t *ai) {
	noise_t *n = (noise_t *)zalloc(sizeof(noise_t));
	n->loop = loop;

	ai->in = n;
	ai->read = audio_in_read;
	ai->stop_read = audio_in_stop_read;
	ai->close = audio_in_close;
}

static int noise_setopt(lua_State *L) {
	lua_getfield(L, 1, "mode");
	const char *mode = lua_tostring(L, -1);

	if (!strcmp(mode, "pink"))
		noise_mode = PINK_NOISE;
	else if (!strcmp(mode, "white"))
		noise_mode = WHITE_NOISE;

	return 0;
}

void luv_noise_init(lua_State *L, uv_loop_t *loop) {
	lua_register(L, "noise_setopt", noise_setopt);
}

