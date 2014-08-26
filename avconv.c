
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>

#include <uv.h>

#include "avconv.h"
#include "utils.h"

typedef struct {
	char token_buf[128];
	int token_len;
	char buf[1024];
	int parse_stat;
	int token_stat;
	int key;
	int got_dur;
} avconv_probe_parser_t;

typedef struct avconv_s {
	void *data;
	int stat;

	uv_process_t *proc;
	uv_pipe_t *pipe[2];

	void *read_buf;
	int   read_len;
	audio_in_read_cb read_done;

	avconv_probe_parser_t probe_parser;
} avconv_t;

/*
 * ==================== probe data format ====================
  
   Input #0, mp3, from '../testdata/10s-1.mp3':
     Metadata:
     artist          : The Smashing Pumpkins
     album_artist    : The Smashing Pumpkins
     disc            : 1
     track           : 9
     title           : Mayonaise
     album           : Siamese Dream
     date            : 1993
     encoder         : Lavf54.20.4
     Duration: 00:00:10.03, start: 0.000000, bitrate: 186 kb/s
     Stream #0.0: Audio: mp3, 44100 Hz, stereo, s16p, 128 kb/s
     Stream #0.1: Video: png, rgb24, 185x185, 90k tbn
     Metadata:
     title           : 
     comment         : Other

 * ===========================================================
 */

enum {
	KEY_DURATION = 1,
};

enum {
	PROBE_WAIT_KEY,
	PROBE_WAIT_VAL_1_1,
};

enum {
	TOKEN_SPACING,
	TOKEN_READING,
};

static float avconv_durstr_to_float(char *s) {
	int hh = 0, ss = 0, mm = 0, ms = 0;
	sscanf(s, "%d:%d:%d.%d", &hh, &mm, &ss, &ms);
	return (float)(hh*3600 + mm*60 + ss) + (float)ms/1e3;
}

static void avconv_on_probe(avconv_t *av, const char *key, void *val) {
	audio_in_t *ai = (audio_in_t *)av->data;

	if (ai->on_probe)
		ai->on_probe(ai, key, val);
}

static void parser_get_token(avconv_t *av, avconv_probe_parser_t *p) {
	static const char *token_durstr = "Duration:";

	if (p->parse_stat == PROBE_WAIT_KEY) {

		if (strncmp(p->token_buf, token_durstr, strlen(token_durstr)) == 0) {
			p->key = KEY_DURATION;
			p->parse_stat = PROBE_WAIT_VAL_1_1;
		}

	} else if (p->parse_stat == PROBE_WAIT_VAL_1_1) {

		if (p->key == KEY_DURATION && !p->got_dur) {
			float dur = avconv_durstr_to_float(p->token_buf);
			avconv_on_probe(av, "dur", &dur);
			p->got_dur++;
		}
		p->key = 0;
		p->parse_stat = PROBE_WAIT_KEY;

	}
}

static void parser_eat(avconv_t *av, avconv_probe_parser_t *p, void *buf, int len) {
	char *s = (char *)buf;

	while (len--) {
		if (isspace(*s)) {
			if (p->token_stat == TOKEN_READING) {
				p->token_buf[p->token_len] = 0;
				parser_get_token(av, p);
				p->token_len = 0;
				p->token_stat = TOKEN_SPACING;
			}
		} else {
			if (p->token_stat == TOKEN_SPACING)
				p->token_stat = TOKEN_READING;
			if (p->token_len < sizeof(p->token_buf)-1)
				p->token_buf[p->token_len++] = *s;
		}
		s++;
	}
}

enum {
	INIT,
	READING,
	KILLING,
	CLOSING_PROC,
	CLOSING_FD1,
	CLOSING_FD2
};

// can_read()
// on_exit()
// INIT -> KILLING -> CLOSING_PROC -> CLOSING_FD1 -> CLOSING_FD2
// data pipe read n < 0 ==> on_exit(); and kill process
// process_exit -> close all handles one by one

static void on_handle_free_done(uv_handle_t *h) {
	avconv_t *av = (avconv_t *)h->data;
	free(h);

	switch (av->stat) {
	case CLOSING_PROC:
		av->stat = CLOSING_FD1;
		uv_close((uv_handle_t *)av->pipe[0], on_handle_free_done);
		break;

	case CLOSING_FD1:
		av->stat = CLOSING_FD2;
		uv_close((uv_handle_t *)av->pipe[1], on_handle_free_done);
		break;

	case CLOSING_FD2:
		av->on_exit(av);
		break;
	}
}

static uv_buf_t probe_alloc_buffer(uv_handle_t *h, size_t len) {
	avconv_t *av = (avconv_t *)h->data;
	return uv_buf_init(av->probe_parser.buf, sizeof(av->probe_parser.buf));
}

static void probe_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	avconv_t *av = (avconv_t *)st->data;

	if (n < 0)
		return;

	parser_eat(av, &av->probe_parser, buf.base, n);
}

static uv_buf_t data_alloc_buffer(uv_handle_t *h, size_t len) {
	avconv_t *av = (avconv_t *)h->data;

	return uv_buf_init(av->data_buf, av->data_len);
}

static void data_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	avconv_t *av = (avconv_t *)st->data;
	
	debug("n=%d", n);

	uv_read_stop(st);

	if (av->stat != READING)
		panic("must be READING state");

	if (n < 0) {
		n = 0;
		uv_process_kill(av->proc, 9);
		av->stat = KILLING;
	} else {
		av->stat = INIT;
	}

	if (av->on_read_done) {
		av->stat = INIT;
		av->on_read_done(av, n);
	}
}

static void proc_on_exit(uv_process_t *proc, int stat, int sig) {
	avconv_t *av = (avconv_t *)proc->data;

	info("sig=%d", sig);

	av->stat = CLOSING_PROC;
	uv_close((uv_handle_t *)proc, on_handle_free_done);
}

static void avconv_read(audio_in_t *ai, void *buf, int len) {
	avconv_t *av = (avconv_t *)ai->in;

	if (av->stat != INIT)
		panic("please call can_read before read");
	av->stat = READING;

	av->data_buf = buf;
	av->data_len = len;

	debug("len=%d", len);

	uv_read_start((uv_stream_t *)av->pipe[0], data_alloc_buffer, data_pipe_read);
}

static void avconv_stop(audio_in_t *ai) {
	avconv_t *av = (avconv_t *)ai->in;

	debug("stop");

	if (av->stat == INIT)
		uv_process_kill(av->proc, 9);
}

static int avconv_can_read(audio_in_t *ai) {
	avconv_t *av = (avconv_t *)ai->in;

	return av->stat == INIT;
}

void audio_in_avconv_init(uv_loop_t *loop, audio_in_t *ai) {
	ai->read = avconv_read;
	ai->stop = avconv_stop;
	ai->can_read = avconv_can_read;

	avconv_t *av = (avconv_t *)zalloc(sizeof(avconv_t));
	av->data = ai;

	uv_process_t *proc = (uv_process_t *)zalloc(sizeof(uv_process_t));
	proc->data = av;
	av->proc = proc;

	int i;
	for (i = 0; i < 2; i++) {
		av->pipe[i] = zalloc(sizeof(uv_pipe_t));
		uv_pipe_init(loop, av->pipe[i], 0);
		uv_pipe_open(av->pipe[i], 0);
		av->pipe[i]->data = av;
	}

	uv_process_options_t opts = {};

	uv_stdio_container_t stdio[3] = {
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)av->pipe[0]},
		{.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)av->pipe[1]},
	};
	opts.stdio = stdio;
	opts.stdio_count = 3;

	char *args[] = {"avconv", "-i", ai->url, "-f", "s16le", "-ar", "44100", "-", NULL};
	opts.file = args[0];
	opts.args = args;
	opts.exit_cb = proc_on_exit;

	int r = uv_spawn(loop, proc, opts);
	info("url=%s spawn=%d pid=%d", ai->url, r, proc->pid);

	uv_read_start((uv_stream_t *)av->pipe[1], probe_alloc_buffer, probe_pipe_read);
}

