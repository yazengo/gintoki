
#pragma once

#include "pipe.h"

void pdirect_read(pipe_t *p);
void pdirect_write(pipe_t *p);
void pdirect_cancel_read(pipe_t *p);
void pdirect_cancel_write(pipe_t *p);

