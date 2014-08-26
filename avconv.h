#pragma once

#include <uv.h>

void avconv_start(uv_loop_t *loop, avconv_t *av, char *fname);
int avconv_can_read(avconv_t *av);
void avconv_read(avconv_t *av, void *buf, int len, void (*done)(avconv_t *, int));
void avconv_stop(avconv_t *av);

