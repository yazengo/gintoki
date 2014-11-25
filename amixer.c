
#include <string.h>
#include <stdlib.h>

#include "luv.h"
#include "pipe.h"
#include "pcm.h"
#include "utils.h"

typedef struct {
	pipe_t *p;
	pipebuf_t *pb;

	queue_t tracks;

	int rdstat, wrstat;
	float vol;
} amixer_t;

typedef struct {
	queue_t q;

	amixer_t *am;

	pipebuf_t *pb;
	pipe_t *p;

	int stat;
	float vol;

	immediate_t im_close;
} track_t;

enum {
	INIT,
	READING,
	WRITING,
	READDONE,
	CLOSING,
	CLOSED,
};

static void amixer_mix(amixer_t *am);

static void track_close(track_t *tr) {
	pipe_close_read(tr->p);
	queue_remove(&tr->q);
	tr->stat = CLOSED;
}

static void track_read_done(pipe_t *p, pipebuf_t *pb) {
	track_t *tr = (track_t *)p->read.data;
	amixer_t *am = tr->am;

	if (pb == NULL) {
		track_close(tr);
		return;
	}
	debug("done p=%p p.type=%d", tr->p, tr->p->type);
	debug("rdstat=%d wrstat=%d", am->rdstat, am->wrstat);

	tr->pb = pb;
	tr->stat = READDONE;

	if (am->rdstat == INIT)
		am->rdstat = READDONE;

	if (am->rdstat == READDONE && am->wrstat == INIT)
		amixer_mix(am);
}

static void track_read(track_t *tr) {
	tr->stat = READING;
	debug("read p=%p p.type=%d", tr->p, tr->p->type);
	pipe_read(tr->p, track_read_done);
}

static void track_add(amixer_t *am, pipe_t *p, track_t *tr) {
	debug("am=%p am.p.data=%p p=%p", am, am->p->read.data, p);

	tr->am = am;
	tr->p = p;
	tr->vol = 1.0;
	p->read.data = tr;
	queue_insert_tail(&am->tracks, &tr->q);

	debug("p=%p type=%d", p, p->type);
	track_read(tr);
}

static void amixer_close(amixer_t *am) {
	debug("close");

	pipe_close_write(am->p);

	am->rdstat = CLOSED;
	am->wrstat = CLOSED;
	
	queue_t *q;
	queue_foreach(q, &am->tracks) {
		track_t *tr = queue_data(q, track_t, q);
		track_close(tr);
	}

	luv_unref(am);
}

static void amixer_write_done(pipe_t *p, int stat) {
	amixer_t *am = (amixer_t *)p->write.data;
	
	debug("done am=%p rdstat=%d wrstat=%d", am, am->rdstat, am->wrstat);

	if (stat < 0) {
		amixer_close(am);
		return;
	}

	am->wrstat = INIT;
	if (am->rdstat == READDONE)
		amixer_mix(am);
}

static void amixer_mix(amixer_t *am) {
	am->pb = pipebuf_new();
	debug("mixbuf p=%p", am->pb);

	memset(am->pb->base, 0, PIPEBUF_SIZE);

	queue_t *q;
	queue_foreach(q, &am->tracks) {
		track_t *tr = queue_data(q, track_t, q);

		if (tr->stat == READDONE) {
			pcm_do_volume(tr->pb->base, PIPEBUF_SIZE, tr->vol * am->vol);
			pcm_do_mix(am->pb->base, tr->pb->base, PIPEBUF_SIZE);
			pipebuf_unref(tr->pb);

			track_read(tr);
		}
	}

	debug("write am=%p am.p.data=%p", am, am->p->write.data);
	am->rdstat = INIT;
	am->wrstat = WRITING;
	pipe_write(am->p, am->pb, amixer_write_done);
}

static int track_setopt(lua_State *L, uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;
	char *op = (char *)lua_tostring(L, 1);
	track_t *tr = (track_t *)p->read.data;

	if (op && !strcmp(op, "getvol")) {
		lua_pushnumber(L, tr->vol);
		return 1;
	}

	if (op && !strcmp(op, "setvol")) {
		tr->vol = lua_tonumber(L, 2);
		return 0;
	}

	return 0;
}

static void track_gc(uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;
	track_t *tr = (track_t *)p->read.data;

	free(tr);
}

static int amixer_track_add(lua_State *L, uv_loop_t *loop, amixer_t *am) {
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));

	p->type = PDIRECT_SINK;
	luv_setgc(p, track_gc);

	track_t *tr = (track_t *)zalloc(sizeof(track_t));
	track_add(am, p, tr);

	luv_pushcclosure(L, track_setopt, p);
	lua_setfield(L, -2, "setopt");

	return 1;
}

static int amixer_setopt(lua_State *L, uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;
	amixer_t *am = (amixer_t *)p->write.data;
	char *op = (char *)lua_tostring(L, 1);

	if (op && !strcmp(op, "track.add")) {
		return amixer_track_add(L, loop, am);
	}

	if (op && !strcmp(op, "setvol")) {
		am->vol = lua_tonumber(L, 2);
		return 0;
	}

	if (op && !strcmp(op, "getvol")) {
		lua_pushnumber(L, am->vol);
		return 1;
	}

	return 0;
}

static void amixer_gc(uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;
	amixer_t *am = (amixer_t *)p->write.data;

	free(am);
}

static int amixer_new(lua_State *L, uv_loop_t *loop) {
	amixer_t *am = (amixer_t *)zalloc(sizeof(amixer_t));
	pipe_t *p = luv_newctx(L, loop, sizeof(pipe_t));

	p->type = PDIRECT_SRC;
	p->write.data = am;
	am->p = p;
	am->vol = 1.0;
	queue_init(&am->tracks);

	luv_setgc(p, amixer_gc);
	
	luv_pushcclosure(L, amixer_setopt, p);
	lua_setfield(L, -2, "setopt");

	debug("am=%p p=%p", am, p);

	return 1;
}

void luv_amixer_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "amixer", amixer_new);
}

