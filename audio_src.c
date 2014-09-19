
#include <uv.h>
#include <stdlib.h>

#include "utils.h"
#include "audio_in.h"
#include "audio_src.h"

#define MAX_SRCS 16
static audiosrc_srv_t *allsrvs[MAX_SRCS];

static void cli_stop(audiosrc_cli_t *ac);

void audiosrc_srv_start(uv_loop_t *loop, audiosrc_srv_t *as) {
	int i, empty = 0;

	for (i = 0; i < MAX_SRCS; i++) {
		if (allsrvs[i]) {
			if (!strcmp(allsrvs[i]->url, as->url))
				panic("same url exists");
		} else
			empty = i;
	}
	allsrvs[i] = as;
	as->i = i;

	info("url=%s", as->url);
}

static void srv_ringbuf_put_done(ringbuf_t *rb, int len) {
	audiosrc_srv_t *as = (audiosrc_srv_t *)rb->data;
}

void audiosrc_srv_write(audiosrc_srv_t *as, void *buf, int len) {
	if (as->ac == NULL) 
		return;
	ringbuf_data_put_force(&as->ac->buf, buf, len);
}

void audiosrc_srv_stop(audiosrc_srv_t *as) {
	info("url=%s", as->url);

	if (as->ac) {
		cli_stop(as->ac);
		as->ac = NULL;
	}
	allsrvs[as->i] = NULL;
}

enum {
	AS_STOPPED,
	AS_INIT,
	AS_READING,
	AS_CLOSING,
};

static void cli_stop(audiosrc_cli_t *ac) {
	switch (ac->stat) {
	case AS_INIT:
	case AS_READING:
		ringbuf_data_cancel_put(&ac->buf);
		ringbuf_data_cancel_get(&ac->buf);
		ac->stat = AS_CLOSING;
		break;
	case AS_CLOSING:
		break;
	default:
		panic("invalid stat=%d", ac->stat);
	}
}

static void fromsrc_ringbuf_get_done(ringbuf_t *rb, int n) {
	audiosrc_cli_t *ac = (audiosrc_cli_t *)rb->data;
	switch (ac->stat) {
	case AS_READING:
	case AS_CLOSING:
		ac->stat = AS_INIT;
		break;
	default:
		panic("stat=%d invalid: must be READING or CLOSING", ac->stat);
	}
	ac->on_read_done(ac->ai, n);
}

static void fromsrc_read(audio_in_t *ai, void *buf, int len, audio_in_read_cb done) {
	audiosrc_cli_t *ac = (audiosrc_cli_t *)ai->in;

	if (ac->stat != AS_INIT)
		panic("stat=%d invalid: must be INIT", ac->stat);

	ac->stat = AS_READING;
	ac->on_read_done = done;
	ringbuf_data_get(&ac->buf, buf, len, fromsrc_ringbuf_get_done);
}

static void fromsrc_stop(audio_in_t *ai) {
	audiosrc_cli_t *ac = (audiosrc_cli_t *)ai->in;
	cli_stop(ac);
}

typedef struct {
	audio_in_t *ai;
	audio_in_close_cb done;
} cli_done_t;

static void fromsrc_close_done(uv_call_t *c) {
	cli_done_t *d = (cli_done_t *)c->data;
	audio_in_t *ai = d->ai;
	audiosrc_cli_t *ac = (audiosrc_cli_t *)ai->in;

	d->done(ai);

	free(ac);
	free(c);
	free(d);
}

static void fromsrc_close(audio_in_t *ai, audio_in_close_cb done) {
	audiosrc_cli_t *ac = (audiosrc_cli_t *)ai->in;
	cli_done_t *d = (cli_done_t *)zalloc(sizeof(cli_done_t));
	uv_call_t *c = (uv_call_t *)zalloc(sizeof(uv_call_t));

	d->ai = ai;
	d->done = done;
	c->data = d;

	uv_call(ac->loop, c);
}

static int fromsrc_can_read(audio_in_t *ai) {
	audiosrc_cli_t *ac = (audiosrc_cli_t *)ai->in;

	return ac->stat == AS_INIT;
}

static int fromsrc_is_eof(audio_in_t *ai) {
	audiosrc_cli_t *ac = (audiosrc_cli_t *)ai->in;

	return ac->stat > AS_READING && ac->buf.len == 0;
}

void audio_in_audiosrc_init(uv_loop_t *loop, audio_in_t *ai) {
	int i;

	if (ai->url == NULL) 
		panic("url must be set");

	ai->read = fromsrc_read;
	ai->stop = fromsrc_stop;
	ai->close = fromsrc_close;
	ai->can_read = fromsrc_can_read;
	ai->is_eof = fromsrc_is_eof;

	audiosrc_cli_t *ac = (audiosrc_cli_t *)zalloc(sizeof(audiosrc_cli_t));
	ac->ai = ai;
	ac->loop = loop;
	ai->in = ac;

	audiosrc_srv_t *as = NULL;

	for (i = 0; i < MAX_SRCS; i++) {
		if (allsrvs[i] && !strcmp(allsrvs[i]->url, ai->url)) {
			as = allsrvs[i];
			break;
		}
	}

	if (as == NULL) {
		info("src '%s' not found", ai->url);
		return;
	}
	if (as->ac) {
		info("src '%s' already binded", ai->url);
		return;
	}
}

