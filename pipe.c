
#include "luv.h"

typedef struct {
} pipecopy_t;

typedef struct pipe_s {
	int type;
	void (*on_sink_data)(struct pipe_s *p, void *buf, int size);
	void (*close)(struct pipe_s *p);
	pipe_t *peer;
	uv_stream_t *st;
	int fd;
	void *data;
	char buf[1024];
} pipe_t;

enum {
	PT_FILTER, 
	PT_STREAM,
	PT_FILE,
	PT_DIRECT
};

static void stream2stream_start(pipe_t *src, pipe_t *sink) {
}

static void file2file_start(pipe_t *src, pipe_t *sink) {
}

static void file2filter_start(pipe_t *src, pipe_t *sink) {
}

static void file2direct_start(pipe_t *src, pipe_t *sink) {
}

void uv_pipecopy_start(pipecopy_t *c, pipe_t *src, pipe_t *sink) {
	switch (src->type) {
	case PT_FILE:
		switch (sink->type) {
		case PT_FILE:
			file2file_start(src, sink);
			break;
		case PT_FILTER:
			file2filter_start(src, sink);
			break;
		case PT_DIRECT:
			file2direct_start(src, sink);
		}
		break;
	}
}

