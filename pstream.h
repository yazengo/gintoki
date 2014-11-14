
#pragma once

#include "pipe.h"

void pstream_read(pipe_t *p, pipe_allocbuf_cb allocbuf, pipe_read_cb done);
void pstream_write(pipe_t *p, uv_buf_t ub, pipe_write_cb done);
void pstream_forcestop(pipe_t *p);
void pstream_close(pipe_t *p);

