
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>

#include <uv.h>

#include "audio_in.h"
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

typedef struct {
	char *fname_tmp;
	char *fname_done;
	uv_pipe_t *pipe;
	uv_fs_t *req_open, *req_read;
	char buf[2048];
	void *data;
} avconv_tail_t;

typedef struct avconv_s {
	void *data;
	int stat;

	int pending_exit:1;

	uv_process_t *proc;
	uv_pipe_t *pipe[2];

	avconv_tail_t *tail;
	
	void *read_buf;
	int   read_len;

	audio_in_t *ai;
	audio_in_read_cb on_read_done;
	audio_in_close_cb on_close;

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
	return (float)(hh*3600 + mm*60 + ss) + (float)ms/100;
}

static void avconv_on_probe(avconv_t *av, const char *key, void *val) {
	audio_in_t *ai = av->ai;

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

static const char *tail_prefix = "tail://";

enum {
	TAIL_INIT,
	TAIL_OPENING_TMP,
	TAIL_READING_TMP,
	TAIL_SEEKING_TMP,
	TAIL_TMP,
};

static int tail_check_url(char *url) {
	return !strncmp(url, tail_prefix, strlen(tail_prefix));
}

static void tail_init(uv_loop_t *loop, avconv_tail_t *tl, char *url) {
	tl->fname_done = strdup(url + strlen(tail_prefix));
	tl->fname_tmp = zalloc(strlen(tl->fname_done) + 8);
	strcpy(tl->fname_tmp, tl->fname_done);
	strcat(tl->fname_tmp, ".tmp");

	tl->pipe = zalloc(sizeof(uv_pipe_t));
	uv_pipe_init(loop, tl->pipe, 0);
	uv_pipe_open(tl->pipe, 0);

	tl->req_open = zalloc(sizeof(uv_fs_t));

	//uv_fs_open(loop, tl->req_open, tl->fname_tmp, );
}

static void tail_close(avconv_tail_t *tl, audio_in_close_cb done) {
	avconv_t *av = (avconv_t *)tl->data;
	done(av->ai);
}

enum {
	INIT,
	READING,

	CLOSING_WAITING_EXIT,
	CLOSING_PROC,
	CLOSING_FD1,
	CLOSING_FD2,
};

static void on_handle_closed(uv_handle_t *h) {
	avconv_t *av = (avconv_t *)h->data;
	free(h);

	switch (av->stat) {
	case CLOSING_FD1:
		av->stat = CLOSING_FD2;
		uv_close((uv_handle_t *)av->pipe[1], on_handle_closed);
		break;

	case CLOSING_FD2:
		av->stat = CLOSING_PROC;
		uv_process_kill(av->proc, 9);
		uv_close((uv_handle_t *)av->proc, on_handle_closed);
		break;

	case CLOSING_PROC:
		av->on_close(av->ai);
		free(av);
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

	return uv_buf_init(av->read_buf, av->read_len);
}

static void data_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	avconv_t *av = (avconv_t *)st->data;
	audio_in_t *ai = (audio_in_t *)av->ai;
	
	debug("n=%d stat=%d", n, av->stat);

	uv_read_stop(st);

	if (n < 0) {
		if (av->pending_exited) {
			av->on_stop_done(av->ai);
		} else {
			av->stat = STOPPING;
		}
	} else {
	}

	av->stat = INIT;
	av->on_read_done(ai, n);
}

static void proc_on_exit(uv_process_t *p, int stat, int sig) {
	avconv_t *av = (avconv_t *)p->data;

	info("sig=%d", sig);

	if (av->stat == STOPPING) {
		av->on_stop_done(av->ai);
	} else {
		av->pending_exited = 1;
	}
}

static void avconv_read(audio_in_t *ai, void *buf, int len, audio_in_read_cb done) {
	avconv_t *av = (avconv_t *)ai->in;

	debug("len=%d", len);

	if (av->stat != INIT)
		panic("stat=%d invalid", av->stat);

	av->stat = READING;
	av->read_buf = buf;
	av->read_len = len;
	av->on_read_done = done;

	uv_read_start((uv_stream_t *)av->pipe[0], data_alloc_buffer, data_pipe_read);
}

static void avconv_stop(audio_in_t *ai, audio_in_stop_cb done) {
	avconv_t *av = (avconv_t *)ai->in;

	if (av->stat != INIT)
		panic("stat=%d invalid", av->stat);

	if (av->pending_exited) {
		uv_call_arg1(av->loop, done, ai);
	} else {
		av->stat = STOPPING;
		uv_process_kill(av->proc, 9);
	}
}

static void avconv_close_fd1(audio_in_t *ai) {
	avconv_t *av = (avconv_t *)ai->in;

	av->stat = CLOSING_FD1;
	uv_close((uv_handle_t *)av->pipe[0], on_handle_closed);
}

static void avconv_close(audio_in_t *ai, audio_in_close_cb done) {
	avconv_t *av = (avconv_t *)ai->in;

	if (av->stat != STOPPING)
		panic("must call after is_eof");

	info("close");

	av->on_close = done;

	if (av->tail)
		tail_close(av->tail, avconv_close_fd1);
	else
		avconv_close_fd1(ai);
}

void audio_in_avconv_init(uv_loop_t *loop, audio_in_t *ai) {
	ai->read = avconv_read;
	ai->stop = avconv_stop;
	ai->close = avconv_close;

	avconv_t *av = (avconv_t *)zalloc(sizeof(avconv_t));
	av->ai = ai;
	ai->in = av;

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

	char *url = ai->url;
	if (tail_check_url(ai->url)) {
		url = "-";

		avconv_tail_t *tl = zalloc(sizeof(avconv_tail_t));
		tail_init(loop, tl, ai->url);
		av->tail = tl;
		tl->data = av;

		stdio[0].flags = UV_CREATE_PIPE;
		stdio[0].data.stream = (uv_stream_t *)tl->pipe;
	}

	char *args[] = {"avconv", "-i", url, "-f", "s16le", "-ar", "44100", "-ac", "2", "-", NULL};
	opts.file = args[0];
	opts.args = args;
	opts.exit_cb = proc_on_exit;

	int r = uv_spawn(loop, proc, opts);
	info("url=%s spawn=%d pid=%d", ai->url, r, proc->pid);

	uv_read_start((uv_stream_t *)av->pipe[1], probe_alloc_buffer, probe_pipe_read);
}

