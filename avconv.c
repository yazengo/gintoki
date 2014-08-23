
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>

#include <uv.h>

#include "avconv.h"
#include "utils.h"

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
			if (av->on_probe)
				av->on_probe(av, "dur", &dur);
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

static void handle_free(uv_handle_t *h) {
	avconv_t *av = (avconv_t *)h->data;
	free(h);

	av->fd_closed_nr++;
	info("closed_nr=%d", av->fd_closed_nr);

	if (av->fd_closed_nr == 2) {
		if (av->on_exit)
			av->on_exit(av);
		if (av->on_free)
			av->on_free(av);
	}
}

static uv_buf_t probe_alloc_buffer(uv_handle_t *h, size_t len) {
	avconv_t *av = (avconv_t *)h->data;
	return uv_buf_init(av->probe_parser.buf, sizeof(av->probe_parser.buf));
}

static void probe_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	avconv_t *av = (avconv_t *)st->data;

	if (n < 0) {
		info("n < 0");
		uv_close((uv_handle_t *)st, handle_free);
		return;
	}

	parser_eat(av, &av->probe_parser, buf.base, n);
}

static uv_buf_t data_alloc_buffer(uv_handle_t *h, size_t len) {
	avconv_t *av = (avconv_t *)h->data;

	return uv_buf_init(av->data_buf, av->data_len);
}

static void data_pipe_read(uv_stream_t *st, ssize_t n, uv_buf_t buf) {
	avconv_t *av = (avconv_t *)st->data;

	if (n < 0) {
		info("n < 0");
		uv_close((uv_handle_t *)st, handle_free);
		return;
	}
	
	uv_read_stop(st);

	av->data_buf = NULL;
	if (av->on_read_done)
		av->on_read_done(av, n);
}

static void proc_on_exit(uv_process_t *puv, int stat, int sig) {
	info("sig=%d", sig);
}

void avconv_start(uv_loop_t *loop, avconv_t *av, char *fname) {

	uv_process_t *puv = (uv_process_t *)zalloc(sizeof(uv_process_t));
	puv->data = av;

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

	char *args[] = {"avconv", "-i", fname, "-f", "s16le", "-ar", "44100", "-", NULL};
	opts.file = args[0];
	opts.args = args;
	opts.exit_cb = proc_on_exit;

	int r = uv_spawn(loop, puv, opts);
	av->pid = puv->pid;
	info("cmd=%s spawn=%d pid=%d", fname, r, av->pid);

	uv_read_start((uv_stream_t *)av->pipe[1], probe_alloc_buffer, probe_pipe_read);
}

void avconv_read(avconv_t *av, void *buf, int len, void (*done)(avconv_t *, int)) {
	if (av->data_buf)
		return;

	av->data_buf = buf;
	av->data_len = len;
	av->on_read_done = done;

	uv_read_start((uv_stream_t *)av->pipe[0], data_alloc_buffer, data_pipe_read);
}

void avconv_stop(avconv_t *av) {
	kill(av->pid, 9);
	av->on_read_done = NULL;
	av->on_probe = NULL;
	av->on_exit = NULL;
}

