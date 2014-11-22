
#pragma once

#include "pipe.h"

void pstream_read(pipe_t *p);
void pstream_write(pipe_t *p);
void pstream_cancel_read(pipe_t *p);
void pstream_cancel_write(pipe_t *p);
void pstream_close(pipe_t *p);

