
#include <string.h>
#include <stdlib.h>

#include "luv.h"
#include "utils.h"
#include "pipe.h"
#include "pdirect.h"

static int do_check_close(pipe_t *p);

pipe_t *pipe_new(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	return p;
}

static void do_read_done(pipe_t *p) {
	p->stat &= ~PS_DOING_R;
	if (p->read.pb == NULL)
		p->stat |= PS_STOPPED_R;

	if (p->read.pb)
		debug("n=%d type=%d", p->read.pb->len, p->type);
	else
		debug("n=nil type=%d", p->type);

	p->pread.done(p, p->read.pb);
	do_check_close(p);
}

static void read_done(pipe_t *p, pipebuf_t *pb) {
	p->read.pb = pb;
	if (p->stat & PS_PAUSED)
		return;
	do_read_done(p);
}

void pipe_read(pipe_t *p, pipe_read_cb done) {
	debug("read type=%d", p->type);

	if (p->stat & PS_DOING_R)
		panic("dont call read() before done");
	p->stat |= PS_DOING_R;
	
	p->read.done = read_done;
	p->pread.done = done;

	switch (p->type) {
	case PDIRECT_SRC:
	case PDIRECT_SINK:
		pdirect_read(p);
		break;
	case PSTREAM_SRC:
		pstream_read(p);
		break;
	default:
		panic("type=%d invalid", p->type);
	}
}

static void do_write_done(pipe_t *p) {
	int stat = p->write.stat;

	p->stat &= ~PS_DOING_W;
	if (stat < 0)
		p->stat |= PS_STOPPED_W;
	
	debug("stat=%d type=%d", stat, p->type);

	p->pwrite.done(p, stat);
	do_check_close(p);
}

static void write_done(pipe_t *p, int stat) {
	p->write.stat = stat;
	if (p->stat & PS_PAUSED)
		return;
	do_write_done(p);
}

void pipe_write(pipe_t *p, pipebuf_t *pb, pipe_write_cb done) {
	if (p->stat & PS_DOING_W)
		panic("dont call write() before done");
	p->stat |= PS_DOING_W;
	
	p->write.pb = pb;
	p->write.done = write_done;
	p->pwrite.done = done;

	debug("n=%d type=%d", pb->len, p->type);

	switch (p->type) {
	case PDIRECT_SRC:
	case PDIRECT_SINK:
		pdirect_write(p);
		break;
	case PSTREAM_SINK:
		pstream_write(p);
		break;
	default:
		panic("type=%d invalid", p->type);
	}
}

void pipe_cancel_read(pipe_t *p) {
	p->stat &= ~PS_DOING_R;

	switch (p->type) {
	case PDIRECT_SRC:
	case PDIRECT_SINK:
		pdirect_cancel_read(p);
		break;
	case PSTREAM_SRC:
		pstream_cancel_read(p);
		break;
	default:
		panic("type=%d invalid", p->type);
	}
}

void pipe_cancel_write(pipe_t *p) {
	p->stat &= ~PS_DOING_W;

	switch (p->type) {
	case PDIRECT_SRC:
	case PDIRECT_SINK:
		pdirect_cancel_write(p);
		break;
	case PSTREAM_SINK:
		pstream_cancel_write(p);
		break;
	default:
		panic("type=%d invalid", p->type);
	}
}

static void do_close(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;
	lua_State *L = luv_state(p);

	debug("close");

	luv_pushctx(L, p);
	lua_getfield(L, -1, "on_closed");
	lua_remove(L, -2);
	if (!lua_isnil(L, -1)) 
		lua_call_or_die(L, 0, 0);
	else
		lua_pop(L, 1);

	if (p->read.buf)
		free(p->read.buf);

	switch (p->type) {
	case PDIRECT_SRC:
	case PDIRECT_SINK:
		pdirect_close(p);
		break;
	case PSTREAM_SRC:
	case PSTREAM_SINK:
		pstream_close(p);
		break;
	default:
		panic("type=%d invalid", p->type);
	}
}

static int do_check_close(pipe_t *p) {
	if (p->stat & PS_CLOSED) {
		debug("closed");
		return 1;
	}

	unsigned need = 0;

	switch (p->type) {
	case PSTREAM_SINK:
		need = PS_STOPPED_W | PS_CLOSING_W;
		break;
	case PSTREAM_SRC:
		need = PS_STOPPED_R | PS_CLOSING_R;
		break;
	case PDIRECT_SINK:
	case PDIRECT_SRC:
		need = PS_STOPPED_R | PS_STOPPED_W | PS_CLOSING_R | PS_CLOSING_W;
		break;
	}

	if ((p->stat & need) != need) {
		debug("stat=%x need=%x", p->stat&need, need);
		return 0;
	}

	debug("close type=%d need=%x stat=%x", p->type, need, p->stat);

	p->stat |= PS_CLOSED;
	p->close.im.data = p;
	p->close.im.cb = do_close;
	set_immediate(luv_loop(p), &p->close.im);

	return 1;
}

static void do_stop_write(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;

	p->stat &= ~PS_DOING_W;
	p->stat |= PS_STOPPED_W;
	p->write.done(p, -1);
}

static void do_stop_read(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;

	p->stat &= ~PS_DOING_R;
	p->stat |= PS_STOPPED_R;
	p->read.done(p, NULL);
}

void pipe_stop(pipe_t *p) {
	if (p->stat & PS_STOPPING)
		return;
	p->stat |= PS_STOPPING;

	if (p->stat & PS_DOING_R) {
		pipe_cancel_read(p);
		p->read.im_stop.data = p;
		p->read.im_stop.cb = do_stop_read;
		set_immediate(luv_loop(p), &p->read.im_stop);
	}

	if (p->stat & PS_DOING_W) {
		pipe_cancel_write(p);
		p->write.im_stop.data = p;
		p->write.im_stop.cb = do_stop_write;
		set_immediate(luv_loop(p), &p->write.im_stop);
	}
}

void pipe_close_read(pipe_t *p) {
	p->stat |= PS_CLOSING_R;

	debug("type=%d", p->type);

	if (!(p->stat & PS_STOPPED_R))
		pipe_stop(p);

	do_check_close(p);
}

void pipe_close_write(pipe_t *p) {
	p->stat |= PS_CLOSING_W;

	debug("type=%d", p->type);

	if (!(p->stat & PS_STOPPED_W))
		pipe_stop(p);

	do_check_close(p);
}

void pipe_pause(pipe_t *p) {
	if (p->stat & PS_STOPPING)
		return;
	p->stat |= PS_PAUSED;
}

static void do_resume_read(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;
	do_read_done(p);
}

static void do_resume_write(immediate_t *im) {
	pipe_t *p = (pipe_t *)im->data;
	do_write_done(p);
}

void pipe_resume(pipe_t *p) {
	if (!(p->stat & PS_PAUSED))
		return;
	p->stat &= ~PS_PAUSED;

	if (p->stat & PS_DOING_R) {
		p->read.im_resume.data = p;
		p->read.im_resume.cb = do_resume_read;
		set_immediate(luv_loop(p), &p->read.im_resume);
	}
	if (p->stat & PS_DOING_W) {
		p->write.im_resume.data = p;
		p->write.im_resume.cb = do_resume_write;
		set_immediate(luv_loop(p), &p->write.im_resume);
	}
}

