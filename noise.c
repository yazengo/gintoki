
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

static void gen(void *buf, int len) {
	while (len > 0) {
		*(uint16_t *)buf = rand()&0x7fff;
		buf += 2;
		len -= 2;
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

