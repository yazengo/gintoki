#pragma once

#include <uv.h>

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
	void (*on_probe)(struct avconv_s *, const char *key, void *val);
	void (*on_read_done)(struct avconv_s *, int);
	void (*on_exit)(struct avconv_s *);
	void (*on_free)(struct avconv_s *);
	int pid;
	void *data;
	uv_pipe_t *pipe[2];

	void *data_buf;
	int data_len;

	int fd_closed_nr;

	avconv_probe_parser_t probe_parser;
} avconv_t;

void avconv_start(uv_loop_t *loop, avconv_t *av, char *fname);
void avconv_read(avconv_t *av, void *buf, int len, void (*done)(avconv_t *, int));
void avconv_stop(avconv_t *av);

