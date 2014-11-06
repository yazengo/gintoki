#include <lua.h>
#include <uv.h>

#include "utils.h"
#include "strbuf.h"
#include "itunes.h"

typedef struct itunes_s {
    uv_loop_t* loop;
    uv_work_t* work;
    strbuf_t* body;
    char* filename;
} itunes_t;

static void itunes_save_start(uv_work_t *w) {
    itunes_t* itunes = (itunes_t*)w->data;

    info("iTunes file name: %s", itunes->filename);
    int fd = open(itunes->filename, O_WRONLY | O_CREAT, S_IRWXU);
    if (fd == -1)
        panic("iTunes file open fail");

    write(fd, itunes->body->buf, itunes->body->length);
    close(fd);
}

static void itunes_save_done(uv_work_t *w, int _) {
    itunes_t* itunes = (itunes_t*)w->data;

    strbuf_free(itunes->body);
    free(itunes->work);
    free(itunes);
}

void itunes_save_body(uv_loop_t* loop, strbuf_t* body, char* filename) {
    itunes_t* itunes = (itunes_t*)zalloc(sizeof(itunes_t));

    itunes->loop = loop;
    itunes->work = (uv_work_t*)zalloc(sizeof(uv_work_t));
    itunes->body = body;
    itunes->filename = filename;

    itunes->work->data = itunes;
    uv_queue_work(loop, itunes->work, itunes_save_start, itunes_save_done);
}

