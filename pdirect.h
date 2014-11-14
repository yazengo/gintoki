
#pragma once

#include "pipe.h"

void pdirect_read(pipe_t *p, int n, pipe_read_cb done);
void pdirect_write(pipe_t *p, uv_buf_t ub, pipe_write_cb done);
void pdirect_forcestop(pipe_t *p);
void pdirect_close(pipe_t *p);
void pdirect_cancel_read(pipe_t *p);
void pdirect_cancel_write(pipe_t *p);

