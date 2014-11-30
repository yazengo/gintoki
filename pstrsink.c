
#include "luv.h"
#include "pipe.h"
#include "strbuf.h"

typedef struct {
	strbuf_t *sb;
	pipe_t *p;
	char *grep;
} pstrsink_t;

static void gc(uv_loop_t *loop, void *_p) {
	pipe_t *p = (pipe_t *)_p;
	pstrsink_t *ss = (pstrsink_t *)p->read.data;

	strbuf_free(ss->sb);
	if (ss->grep)
		free(ss->grep);
	free(ss);
}

static int indexn(char *s, int n, char ch) {
	int i;

	for (i = 0; i < n; i++) {
		if (*s == ch)
			return i;
		s++;
	}
	return -1;
}

static void grep_newline(pstrsink_t *ss) {
	char *s = ss->sb->buf;
	s[ss->sb->length] = 0;

	debug("s=%s grep=%s", s, ss->grep);
	if (strstr(s, ss->grep) != NULL) {
		lua_pushstring(luv_state(ss->p), s);
		luv_callfield(ss->p, "grep_cb", 1, 0);
	}
	strbuf_reset(ss->sb);
}

static void grep_parse(pstrsink_t *ss, void *buf, int len) {
	int i = 0;
	for (;;) {
		int pos = indexn(buf + i, len - i, '\n');
		debug("pos=%d", pos);
		if (pos != -1) {
			strbuf_append_mem(ss->sb, buf + i, pos);
			grep_newline(ss);
		} else {
			strbuf_append_mem(ss->sb, buf + i, len - i);
			break;
		}
		i += pos + 1;
	}
}

static void read_eof(pstrsink_t *ss) {
	pipe_t *p = ss->p;

	if (ss->grep == NULL) {
		debug("close n=%d", ss->sb->length);
		lua_pushlstring(luv_state(p), ss->sb->buf, ss->sb->length);
		luv_callfield(p, "done_cb", 1, 0);
	} else {
		grep_newline(ss);
	}

	pipe_close_read(p);
}

static void read_done(pipe_t *p, pipebuf_t *pb) {
	pstrsink_t *ss = (pstrsink_t *)p->read.data;

	if (pb == NULL) {
		read_eof(ss);
		return;
	}

	if (ss->grep == NULL) {
		debug("sinkall.read n=%d sb.len=%d", pb->len, ss->sb->length);
		strbuf_append_mem(ss->sb, pb->base, pb->len);
	} else {
		debug("grep.read n=%d", pb->len);
		grep_parse(ss, pb->base, pb->len);
	}

	pipebuf_unref(pb);
	pipe_read(p, read_done);
}

// pstrsink() -- all
// pstrsink('grep', 'Duration:') -- grep the line has 'Duration:'
static int luv_pstrsink(lua_State *L, uv_loop_t *loop) {
	pipe_t *p = (pipe_t *)luv_newctx(L, loop, sizeof(pipe_t));
	pstrsink_t *ss = (pstrsink_t *)zalloc(sizeof(pstrsink_t));

	char *mode = (char *)lua_tostring(L, 1);
	if (mode && !strcmp(mode, "grep")) {
		char *grep = (char *)lua_tostring(L, 2);
		if (grep)
			ss->grep = strdup(grep);
	}

	p->type = PDIRECT_SINK;
	p->read.data = ss;
	ss->p = p;

	ss->sb = strbuf_new(4096);
	luv_setgc(p, gc);

	pipe_read(p, read_done);

	return 1;
}

void luv_pstrsink_init(lua_State *L, uv_loop_t *loop) {
	luv_register(L, loop, "pstrsink", luv_pstrsink);
}

