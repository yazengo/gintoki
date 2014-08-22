
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>

#include <uv.h>

#include "avconv.h"
#include "utils.h"

static void handle_free(uv_handle_t *h) {
	free(h);
}

static uv_buf_t probe_alloc_buffer(uv_handle_t *h, size_t len) {
	avconv_t *av = (avconv_t *)h->data;
	return uv_buf_init((void *)&av->probe, sizeof(av->probe));
}

static void probe_pipe_read(uv_stream_t *st, ssize_t nread, uv_buf_t buf) {
	avconv_t *av = (avconv_t *)st->data;

	if (nread < 0) {
		uv_close((uv_handle_t *)st, handle_free);
		return;
	}

	if (nread != sizeof(av->probe)) 
		return;

	info("probed rate=%d dur=%f", av->probe.rate, av->probe.dur);
	if (av->on_probed)
		av->on_probed(av);
}

static uv_buf_t data_alloc_buffer(uv_handle_t *h, size_t len) {
	avconv_t *av = (avconv_t *)h->data;

	return uv_buf_init(av->data_buf, av->data_len);
}

static void data_pipe_read(uv_stream_t *st, ssize_t nread, uv_buf_t buf) {
	avconv_t *av = (avconv_t *)st->data;

	uv_read_stop(st);

	info("n=%d", nread);
	if (nread < 0)
		uv_close((uv_handle_t *)st, handle_free);
	
	av->data_buf = NULL;

	if (av->on_read_done)
		av->on_read_done(av, nread);
}

static void proc_on_exit(uv_process_t *puv, int stat, int sig) {
	avconv_t *av = (avconv_t *)puv->data;

	info("sig=%d", sig);

	if (av->on_exit)
		av->on_exit(av);

	uv_close((uv_handle_t *)puv, handle_free);
}

void avconv_start(uv_loop_t *loop, avconv_t *av, char *fname) {
	info("fname=%s", fname);

	uv_process_t *puv = (uv_process_t *)zalloc(sizeof(uv_process_t));
	puv->data = av;

	int i;
	for (i = 0; i < 2; i++) {
		av->pipe[i] = malloc(sizeof(uv_pipe_t));
		uv_pipe_init(loop, av->pipe[i], 0);
		uv_pipe_open(av->pipe[i], 0);
		av->pipe[i]->data = av;
	}

	uv_process_options_t opts = {};

	uv_stdio_container_t stdio[4] = {
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)av->pipe[0]},
		{.flags = UV_IGNORE},
		{.flags = UV_CREATE_PIPE, .data.stream = (uv_stream_t *)av->pipe[1]},
	};
	opts.stdio = stdio;
	opts.stdio_count = 4;

	char *args[] = {"avconv-sugr", "-i", fname, "-ctrl_dump_fd", "3", "-f", "s16le", "-ar", "44100", "-", NULL};
	opts.file = args[0];
	opts.args = args;
	opts.exit_cb = proc_on_exit;

	if (0) {
		char cmd[1024] = {};
		for (i = 0; args[i]; i++) {
			strcat(cmd, args[i]);
			strcat(cmd, " ");
		}
		info("cmd=%s", cmd);
	} 

	int r = uv_spawn(loop, puv, opts);
	info("spawn=%d", r);

	uv_read_start((uv_stream_t *)av->pipe[0], data_alloc_buffer, data_pipe_read);
	uv_read_start((uv_stream_t *)av->pipe[1], probe_alloc_buffer, probe_pipe_read);
	uv_read_stop((uv_stream_t *)av->pipe[0]);
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
	av->on_probed = NULL;
}

