
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
	allsrvs[empty] = as;
	as->i = empty;

	info("url=%s", as->url);
}

void audiosrc_srv_write(audiosrc_srv_t *as, void *buf, int len) {
	debug("len=%d", len);
	if (as->ac == NULL) 
		return;
	pipebuf_write(&as->ac->buf, buf, len);
}

void audiosrc_srv_stop(audiosrc_srv_t *as) {
	info("url=%s i=%d", as->url, as->i);

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
	debug("stopped");

	switch (ac->stat) {
	case AS_STOPPED:
	case AS_INIT:
	case AS_READING:
		pipebuf_close_read(&as->ac->buf);
		ac->stat = AS_CLOSING;
		break;
	case AS_CLOSING:
		break;
	default:
		panic("invalid stat=%d", ac->stat);
	}
}

static void fromsrc_pipebuf_get_done(pipebuf_t *pb, int n) {
	audiosrc_cli_t *ac = (audiosrc_cli_t *)pb->data;

	debug("n=%d", n);

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

	debug("n=%d stat=%d", len, ac->stat);

	if (ac->stat != AS_INIT)
		panic("stat=%d invalid: must be INIT", ac->stat);

	ac->stat = AS_READING;
	ac->on_read_done = done;

	pipebuf_read(&as->ac->buf, buf, len, fromsrc_pipebuf_read_done);
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
	debug("closed");

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

	debug("close");

	d->ai = ai;
	d->done = done;
	c->data = d;
	c->done_cb = fromsrc_close_done;

	uv_call(ac->loop, c);
}

static int fromsrc_can_read(audio_in_t *ai) {
	audiosrc_cli_t *ac = (audiosrc_cli_t *)ai->in;

	debug("stat=%d", ac->stat);

	return ac->stat == AS_INIT;
}

static int fromsrc_is_eof(audio_in_t *ai) {
	audiosrc_cli_t *ac = (audiosrc_cli_t *)ai->in;

	debug("stat=%d", ac->stat);

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
	ac->stat = AS_INIT;

	ringbuf_init(&ac->buf, loop);
	ac->buf.data = ac;

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
	as->ac = ac;

	debug("starts");
}

