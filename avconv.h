#pragma once

#include <uv.h>

typedef struct {
	int32_t rate;
	float dur;
} probe_info_t;

typedef struct avconv_s {
	void (*on_probed)(struct avconv_s *);
	void (*on_read_done)(struct avconv_s *, int);
	void (*on_exit)(struct avconv_s *);
	void (*on_free)(struct avconv_s *);
	int pid;
	void *data;
	uv_pipe_t *pipe[2];

	void *data_buf;
	int data_len;

	probe_info_t probe;

} avconv_t;

void avconv_start(uv_loop_t *loop, avconv_t *av, char *fname);
void avconv_read(avconv_t *av, void *buf, int len, void (*done)(avconv_t *, int));
void avconv_stop(avconv_t *av);

