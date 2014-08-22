
#include <uv.h>
#include <lua.h>

#include "utils.h"
#include "avconv.h"
#include "audio_mixer.h"
#include "audio_out.h"

enum {
	TRACK_STOPPED,
	TRACK_BUFFERING,
	TRACK_PLAYING,
	TRACK_PAUSED,
	TRACK_PAUSING_VOL_DOWN,
	TRACK_RESUMING_VOL_UP,
	TRACK_FADING_OUT,
};

#define RINGBUF_SIZE (1024*16)
#define PLAYBUF_SIZE (1024*4)
#define TRACKS_NR 2

typedef struct {
	char buf[RINGBUF_SIZE];
	int head, tail, len;
} ringbuf_t;

static void ringbuf_clear(ringbuf_t *b) {
	b->head = b->tail = b->len = 0;
}

static void ringbuf_data_ahead_get(ringbuf_t *b, void **_buf, int *_len) {
	int len = RINGBUF_SIZE - b->tail;
	if (b->len < len)
		len = b->len;
	len &= ~1;
	*_len = len;
	*_buf = &b->buf[b->tail];
}

static void ringbuf_space_ahead_get(ringbuf_t *b, void **_buf, int *_len) {
	int len = RINGBUF_SIZE - b->head;
	if ((RINGBUF_SIZE - b->len) < len)
		len = RINGBUF_SIZE - b->len;
	len &= ~1;
	*_len = len;
	*_buf = &b->buf[b->head];
}

static void ringbuf_push_head(ringbuf_t *b, int len) {
	b->head = (b->head + len) % RINGBUF_SIZE;
	b->len += len;
}

static void ringbuf_push_tail(ringbuf_t *b, int len) {
	b->tail = (b->head + len) % RINGBUF_SIZE;
	b->len -= len;
}

static inline int16_t clip_int16_c(int a) {
    if ((a+0x8000) & ~0xFFFF) return (a>>31) ^ 0x7FFF;
    else                      return a;
}

static void pcm_do_volume(void *_out, int len, float fvol) {
	int16_t *out = (int16_t *)_out;
	len /= 2;

	int vol = fvol * 255;

	if (vol == 255)
		return;
	if (vol > 255)
		vol = 255;
	if (vol <= 0)
		vol = 0;

	while (len--) {
		*out = clip_int16_c((*out * vol) >> 8);
		out++;
	}
}

static void pcm_do_mix(void *_out, void *_in, int len) {
	int16_t *out = (int16_t *)_out;
	int16_t *in = (int16_t *)_in;

	len /= 2;
	while (len--) {
		*out += *in;
		out++;
		in++;
	}
}

struct audio_mixer_s;

typedef struct {
	struct audio_mixer_s *am;
	avconv_t *av;
	int stat;
	ringbuf_t buf;
	float vol;
} audio_track_t;

typedef struct audio_mixer_s {
	audio_track_t tracks[TRACKS_NR];
	audio_out_t *ao;
	ringbuf_t mixbuf;
	float vol;

	uv_loop_t *loop;
	lua_State *L;
} audio_mixer_t;

/*
audio.stop = func () {
}
audio.pause = func () {
}
audio.resume = func () {
}
audio.play = func (url) {
}
*/

static void emit() {
}

static audio_mixer_t *lua_getam(lua_State *L) {
	audio_mixer_t *am;
	void *ud = lua_touserdata(L, lua_upvalueindex(1));
	memcpy(&am, ud, sizeof(am));
	return am;
}

static void track_change_stat(audio_track_t *tr, int stat) {
	tr->stat = stat;
	emit(tr->am, "stat_change");
}

static void check_all_tracks(audio_mixer_t *am);

static void avconv_on_exit(avconv_t *av) {
	audio_track_t *tr = (audio_track_t *)av->data;

	tr->av = NULL;
	track_change_stat(tr, TRACK_STOPPED);

	emit(tr->am, "done");
}

static void avconv_on_free(avconv_t *av) {
	free(av);
}

static void avconv_on_probed(avconv_t *av) {
	audio_track_t *tr = (audio_track_t *)av->data;
}

static void avconv_on_read_done(avconv_t *av, int len) {
	audio_track_t *tr = (audio_track_t *)av->data;

	if (len < 0)
		return;

	ringbuf_push_head(&tr->buf, len);
	check_all_tracks(tr->am);
}

static void audio_out_on_play_done(audio_out_t *ao, int len) {
	audio_mixer_t *am = (audio_mixer_t *)ao->data;

	ringbuf_push_tail(&am->mixbuf, len);
	check_all_tracks(am);
}

static void check_all_tracks(audio_mixer_t *am) {
	int playlen = PLAYBUF_SIZE;
	audio_track_t *playtr[TRACKS_NR];
	int canplay = 0;
	int i;

	void *mixbuf; int mixlen;
	ringbuf_space_ahead_get(&am->mixbuf, &mixbuf, &mixlen);

	if (mixlen < playlen)
		playlen = mixlen;

	for (i = 0; i < TRACKS_NR; i++) {
		audio_track_t *tr = &am->tracks[i];

		if (tr->stat == TRACK_STOPPED)
			continue;

		void *spacebuf; int spacelen;
		ringbuf_space_ahead_get(&tr->buf, &spacebuf, &spacelen);
		if (spacelen > 0) {
			avconv_read(tr->av, spacebuf, spacelen, avconv_on_read_done);
		}

		void *databuf; int datalen;
		ringbuf_data_ahead_get(&tr->buf, &databuf, &datalen);
		if (datalen > 0) {
			playtr[canplay++] = tr;
			if (datalen < playlen)
				playlen = datalen;
		}
	}

	if (!canplay)
		return;

	for (i = 0; i < canplay; i++) {
		audio_track_t *tr = playtr[i];

		void *databuf; int datalen;
		ringbuf_data_ahead_get(&tr->buf, &databuf, &datalen);
		ringbuf_push_tail(&tr->buf, playlen);

		if (i == 0) 
			memcpy(mixbuf, databuf, playlen);
		else
			pcm_do_mix(mixbuf, databuf, playlen);
	}

	pcm_do_volume(mixbuf, playlen, am->vol);
	ringbuf_push_head(&am->mixbuf, playlen);
	audio_out_play(am->ao, mixbuf, playlen, audio_out_on_play_done);
}

static int audio_play(lua_State *L) {
	audio_mixer_t *am = lua_getam(L);
	audio_track_t *tr = &am->tracks[0];
	char *fname = (char *)lua_tostring(L, 1);

	if (tr->av) {
		avconv_stop(tr->av);
		tr->av = NULL;
	}

	ringbuf_clear(&tr->buf);

	tr->am = am;
	tr->av = (avconv_t *)zalloc(sizeof(avconv_t));
	tr->av->data = tr;
	tr->av->on_probed = avconv_on_probed;
	tr->av->on_exit = avconv_on_exit;
	tr->av->on_free = avconv_on_free;
	avconv_start(am->loop, tr->av, fname);

	track_change_stat(tr, TRACK_BUFFERING);
	check_all_tracks(am);

	return 0;
}

void audio_mixer_init(lua_State *L, uv_loop_t *loop) {
	audio_mixer_t *am = (audio_mixer_t *)zalloc(sizeof(audio_mixer_t));
	am->loop = loop;
	am->L = L;

	am->vol = 1.0;

	am->ao = (audio_out_t *)zalloc(sizeof(audio_out_t));
	am->ao->data = am;
	audio_out_init(loop, am->ao, 44100);

	// audio = {}
	lua_newtable(L);
	lua_setglobal(L, "audio");

	// audio.play = [native function]
	lua_getglobal(L, "audio");
	void *ud = lua_newuserdata(L, sizeof(am));
	memcpy(ud, &am, sizeof(am));
	lua_pushcclosure(L, audio_play, 1);
	lua_setfield(L, -2, "play");
	lua_pop(L, 1);
}

