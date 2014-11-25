
require('luv');

aout

apipe(aout, mixer)

p.pause()
p.resume()

m = amixer()
p = m.add()
m.remove(p)
m.has(p)

m.setvol(13)
m.getvol() -- 1

m.highlight(p)

ctrl.mixer = amixer()
ctrl.psong = apipe()
ctrl.pmain = pswitcher(ctrl.psong)

popen('ls /tmp').closed(function ()
end)

pcurl('http://g.cn').closed(function ()
end)

system('ls', function (r)
	info('ret=', r)
end)

popen('ls', function (code)
end).readall(function (r)
end)

apipe(popen('ls'), popen('cat >ls.log'))

apipe(fopen('a.mp3'), popen('avconv -i - -f s16le -ar 44100 -ac 2 -'), aout).closed(function ()
end)

ctrl.loadsong = function ()
	radio.next(function (s)
		ctrl.song = s
		ctrl.playsong()
	end)
end

ctrl.mixer = amixer()
ctrl.msong = amixer()
apipe(ctrl.msong, ctrl.mixer)

apipe = function (...)
	local nodes = {...}
	local n = table.maxn(nodes)
	local r = {}

	r.refcnt = 0

	if nodes[1].stdin then
		r.stdin = nodes[1].stdin
		r.refcnt = r.refcnt + 1
	end

	if nodes[n].stdout then
		r.stdout = nodes[n].stdout
		r.refcnt = r.refcnt + 1
	end

	for i = 1, n-1 do
		local src = nodes[i].stdout
		local sink = nodes[i+1].stdin
		if not src or not sink then
			panic('must specify src and sink')
		end
		pcopy(src, sink)
	end

	return r
end

amixer = function (...)
	m = {
		srcs[10],
	}

	srcs[0 ~ 10].closed(function (this)
		m.remove(this)
	end)

end

--[[

Close:

close(p) refcnt=1
close(p) refcnt=2

Pipe queue:

StreamPipe:

pending_readdone
pending_read
pending_writedone
pending_write
paused
stopped
cancelled

read() {
	enque(READ)
}
cancel() {
}

init
ioing
ioing|paused
iodone|paused
iodone|resuming
stopped
ioing|stopped
ioing|stopped|closed

proc() {
	q = deque()
	q == PAUSE && p.setpaused()
	q == RESUME && {
		pending_readdone && pending_read {
			clear both
			readdone()
		}
	}
	q == READ && {
		if paused { return }
		if stopped { readdone(-1) }
	}
	q == STOP && {
		p.setstopped()
	}
	q == CLOSE && {
		p.setclosed()
	}
	q == CANCEL && {
		p.setcancelled()
	}
	q == READDONE && {
		if p.cancelled() {
		}
	}
}

DirectPipe:

pending_read
pending_write


--]]

--
-- node = {
--   stdin  = [native pipe_t] or nil,
--   stdout = [native pipe_t] or nil,
--   stderr = [native pipe_t] or nil,
--   closed = function () end,
-- }
--
-- pcopy(node.stdout, node2.stdin)
--
-- node.refcnt = 3
--
-- node.stdin.closed(function ()
--   node.unref(node.close_funcs)
-- end)
--
-- node.stdout.closed(function ()
--   node.unref(node.close_funcs)
-- end)
--
-- node.stderr.closed(function ()
--   node.unref(node.close_funcs)
-- end)
--
-- node.closed(function ()
-- end)
--
-- {
--    stdout = [native pipe_t] or nil,
-- }
--
-- [urlopen:1  -> file:??]
-- [adecoder:0 -> avconv:stdin]
--
-- int oldfd = open("app_log", O_RDWR|O_CREATE, 0644);
-- dup2(oldfd, 1);
-- close(oldfd);
--

--[[

UV Context:

uvctx_new();
uvctx_ref();
uvctx_unref();
uvctx_setgc();

luv_pipe() {
	p = pipe_new();
	luv_fromctx(p);
	luv_setgc(p, onclose);
}

API of pipe:

pipe_read(p, done)
pipe_write(p, done)
pipe_pause(p)
pipe_resume(p)
pipe_stop(p)
pipe_cancel(p)
pipe_close(p)
pipe_copy(src, sink)
pipe_is_stopped(p)
pipe_is_paused(p)

pdirect_read(p)   # only for PDIRECT_SRC
pdirect_write(p)  # only for PDIRECT_SINK

Lua API of pipe

pipe.pause(p)
pipe.resume(p)
pipe.stop(p)
pipe.copy(src, sink, {
	align = 8,
	done = function () end,
})

Types of pipe:

PSTREAM_SINK
PSTREAM_SRC
PDIRECT_SINK
PDIRECT_SRC

States of pipe stream:

INIT
READING
WRITING

read() -> done(0) -> read() -> done(-1) -> close()
write() -> done(0) -> write() -> done(-1) -> close()
pause()/resume()/stop()/cancel() at any time

when pause() called. read() and write() will never done. until resume() called.
when cancel() called. in-progress operations will be cancelled.

enum {
	P_RUNNING
	P_CLOSING
	P_CLOSED
};

typedef struct {
	struct {
		struct {
			uv_buf_t ub; // exists when waiting write done
			done;   // exists when waiting write done
		} write;
		struct {
			int len;
			done;    // 
		} read;
		struct {
			done;
		} shutdown;
		int stat;
	} direct;

	struct {
		readdone;
		writedone;
	} stream;

	allocbuf;
	readdone;
	writedone;
} pipe_t;



pipe_allocbuf();

// pdirect functions
p->type = PT_DIRECT;
pdirect_read(p, len, done) {
	if p.closenr == 2 or p.read.done {
		panic()
	}
	p.read.done = done
	p.read.len = len

	set_immediate(func () {
		w = d.write
		r = d.read

		do_trans = func (r, w) {
			if r.len > w.ub.len {
				r.len = w.ub.len
			}

			rub.base = w.ub.base
			rub.len = r.len

			w.ub.base += r.len
			w.ub.len -= r.len

			r.done(rub)
			r.done = nil
			r.len = 0

			if w.ub.len == 0 { 
				w.done(0)
			}
		}

		if p.closecnt > 0 {
			p.read.done(-1)
			return
		}
		if w.ub.len > 0 {
			do_trans(r, w)
		}
	})
}

pdirect_write(p, ub, done) {
	if p.closenr == 2 or p.write.done {
		panic()
	}
	p.write.done = done
	p.write.ub = ub

	set_immediate(func () {
		w = p.write
		r = p.read

		if p.closenr > 0 {
			p.write.done(-1)
			return
		}
		if r.done {
			do_trans(r, w)
		}
	})
}

pdirect_stop(p) {
	pdirect_close(p)
}

pdirect_close(p) {
	if p.closenr == 2 {
		panic()
	}
	p.closenr++

	set_immediate(func () {
		if w.done {
			w.done(-1)
			w.done = nil
		}
		if r.done {
			r.done(-1)
			r.done = nil
		}
		if p.closenr == 2 {
			luv_pushctx(p)
			lua_getfield("on_closed")
			lua_call();
		}
	})
}

pstream_stop(p) {
	uv_shutdown(p.st);
}

pstream_close(p) {
	uv_close(p.st)
}

- pipe_write -> [direct sink] <- pipe_read - pipe_write -> [direct src] <- pipe_read

// pipe function
pipe_read(p, allocbuf, readdone);
// if allocbuf == NULL use the buffer he pass me, dont do memcpy
// if allocbuf != NULL. if allocbuf == pipe_allocbuf then alloc buffer in pipe_t.buf.
//   or user pass your own allocbuf
//   pipe_t.buf will be released when pipe_t gc
pipe_read(p, allocbuf, readdone) {
	if p.type == PT_DIRECT {
		if allocbuf {
			to = allocbuf()
			pdirect_read(p, to.len, func (ub) {
				if ub.len < 0 { readdone(-1) }
				memcpy(to.base, ub.base, ub.len);
				readdone(ub)
			})
		} else {
			pdirect_read(p, -1, func (ub) {
				readdone(ub)
			})
		}
	}
	if p.type == PT_STREAM {
		pstream_read(p, allocbuf, readdone)
	}
}

pipe_write(p, ub, writedone) {
	if p.type == PT_DIRECT {
		pdirect_write(p, ub, func (stat) {
			if stat < 0 { writedone(-1) }
			writedone(0)
		})
	}
	if p.type == PT_STREAM {
		pstream_write(p, ub, writedone);
	}
}

pipe_close(p) {
	if p.type == PT_DIRECT {
		pdirect_close(p)
	}
	if p.type == PT_STREAM {
		pstream_close(p)
	}
}

pipe_stop(p) {
	if p.type == PT_DIRECT {
		pdirect_stop(p)
	}
	if p.type == PT_STREAM {
		pstream_stop(p)
	}
}

// pipe copy
//   close next sink if read src failed
//   close next sink if write sink failed
loop = func () {
	pipe_read(src, NULL, func (ub) {
		if ub.len < 0 {
			pipe_close(sink)
			return
		}
		pipe_write(sink, ub, func (stat) {
			if stat < 0 {
				pipe_close(sink)
				return
			}
			loop()
		})
	})
}

pipe_copy(src, sink) {
	loop()
}

// a simple filter
sink->type = PT_DIRECT
src->type = PT_DIRECT

func writedone(src, stat) {
	if stat < 0 {
		pipe_close(src);
		pipe_close(sink);
	} else {
		loop()
	}
}

func loop() {
	pipe_read(sink, NULL, func (ub) {
		if ub.len < 0 {
			pipe_close(sink);
			pipe_close(src);
			return;
		}
		do_volume(ub)
		pipe_write(src, ub, writedone)
	})
}

// a audio out sink
sink->type = PT_DIRECT

func play(ub, done) {
}

func loop() {
	pipe_read(sink, NULL, func (ub) {
		play(ub, loop)
	})
}

// a audio generate src
src->type = PT_DIRECT

func loop() {
	ub = generate()
	pdirect_write(src, ub, loop)
}

// a audio mixer src
func done() {
	m->src[0]
}

m.mixlen = PIPE_BUFSIZE
m.mixbuf = char buf[m.mixlen];
m.srcs = {
	buf char[1024],
	ub uv_buf_t,
	p pipe_t,
}
m.waiting = true
m.sink = new pipe_t(type=PT_DIRECT)

func allocbuf(s, len) {
	return s.buf
}

func can_mix(m) {
	return [max ub.len in srcs] > 0
}

func do_mix(m) {
	maxlen = [calc maxlen]
	for s in srcs {
		s.ub.len -= len;
	}
	pipe_write(m.sink, mix_ub, writedone)
}

func writedone(s, stat) {
	if can_mix(m) {
		do_mix(m)
	} else {
		m.waiting = true
	}
	do_read()
}

func m.del(s) {
	if s.ub.len > 0 {
		m.ub.len
	}
}

func readdone(s, ub) {
	if ub.len < 0 {
		m.del(s)
		pipe_close(s)
		return
	}
	
	s.ub = ub

	if m.waiting {
		do_mix(m)
		m.waiting = false
	}

	pipe_read(s, allocbuf, readdone)
}

for s in srcs {
	pipe_read(s, allocbuf, readdone)
}

]]--

adecoder = function (on_meta)
	local p = popen('avconv -i - -f s16le -ar 44100 -ac 2 -')
	avprobeparser(p.stderr, on_meta)
	return p.stdout
end

urlopen = function (url)
	if string.hasprefix(url, 'http') then
		return pcurl(url)
	else
		return fopen(url)
	end
end

ctrl.playsong = function ()
	urlopen('', function (p)
		apipe(p, adecoder(), ...)
	end)
	ctrl.psong = apipe(urlopen(ctrl.song.url), adecoder(), afilter{fadein=300}).closed(function ()
		ctrl.psong = nil
		ctrl.song = nil
		ctrl.loadsong()
	end)
	ctrl.msong.add(ctrl.psong)
end

ctrl.next = function ()
	if ctrl.psong then
		ctrl.psong.append(afilter{fadeout=300,exit=true})
	end
end

ctrl.radio_reset = function ()
	if ctrl.psong then
		ctrl.msong.remove(ctrl.psong)
		ctrl.psong.close()
	end
end

ctrl.loadsong()

ctrl.insert_n = 0

ctrl.insert = function (p, done)
	if ctrl.insert_n then
		ctrl.mixer.remove(ctrl.insert_p)
		ctrl.insert_p.close_read()
	end

	if ctrl.insert_n == 0 then
		ctrl.mixer.remove(ctrl.msong)
	end
	ctrl.mixer.add(p)
	ctrl.insert_p = p
	ctrl.insert_n = ctrl.insert_n + 1

	p.closed(function ()
		ctrl.mixer.remove(p)
		ctrl.insert_n = ctrl.insert_n - 1
		if ctrl.insert_n == 0 then
			ctrl.insert_p = nil
			ctrl.mixer.add(ctrl.msong)
		end
		done()
	end)
end

airplay.on_start = function (p)
	ctrl.insert(p, function ()
	end)
end

burnin.on_start = function (p)
	ctrl.insert(p, function ()
	end)
end

servermsg.pipe = function ()
end

alert.on_trigger = function ()
	ctrl.mixer.add(adecoder('alert.mp3'), {vol=30})
end

ctrl.on_pause = function () ctrl.pmain.pause() end
ctrl.on_resume = function () ctrl.pmain.resume() end

m = amixer()
m.add(tts('Somebody ...'), {vol=30})
m.add(adecoder(url), {vol=40})

http.server {
	handler = function (req, res)
		m.fadein(function ()
			m.add(res.body).on_end(function ()
				m.fadeout(function ()
					m.remove(res.body)
					res.send{}
				end)
			end)
		end, {time=300})
	end,
}

http.server {
	parse_headers = true,
	handler = function (req, res)
		apipe(req.body, fopen('file', 'w+')).closed(function ()
			res.send{}
		end)
	end,
}

tcp.server {
	host = 'localhost',
	port = 8811,
	handler = function (req, res)
		airplay.on_start(req.body, function ()
			res.close()
		end)
	end,
}


local R = {}

R.canceller = function ()
	local C = {}

	C.cancel = function ()
		C.cancelled = true
	end

	C.wrap = function (func)
		return function ()
			if not C.cancelled then func() end
		end
	end

	return C
end

R.new_station = function (S)
	local call2 = function (a, b)
		return function (...)
			a(...)
			b(...)
		end
	end

	S.songs = {}
	S.songs_i = 1

	S.skip = function ()
		if s.on_skip then s.on_skip() end
	end

	S.song_next = function ()
		if S.songs_i <= table.maxn(S.songs) then
			S.songs_i = S.songs_i+1
			return S.songs[S.songs_i-1]
		end
	end

	S.restart_done = function (songs)
		table.append(S.songs, songs)
		if S.next_cb then
			S.next_cb(S.song_next())
			S.next_cb = nil
		end
	end

	S.restart_fail = function (...)
	end

	S.restart = function ()
		if S.fetching then
			S.fetching.cancel()
		end
		local task = R.canceller()
		task.done = call2(task.on_done, C.restart_done)
		task.fail = C.restart_fail
		S.fetching = task
		return task
	end

	S.fetch_done = function (songs)
		table.append(S.songs, songs)
		S.next_cb(S.song_next())
		S.fetching = nil
		S.next_cb = nil
	end

	S.fetch_fail = function (err)
		S.fetching = nil
	end

	S.cancel_next = function ()
		S.next_cb = nil
	end

	S.next = function (o, done)
		local song = S.song_next()
		if song then 
			set_timeout(function () done(song) end, 0)
			return
		end
		S.next_o = o
		S.next_cb = done
		if not S.fetching then 
			local task = S.fetch()
			task.done = call2(S.fetch_done, task.on_done)
			task.fail = S.fetch_fail
			S.fetching = task
		end
	end

	return S
end

-- urls = {'http://...', ...}
-- option = {loop=true/false, done=[function]}
R.new_songlist = function (urls, o)
	local S = {}

	S.list = urls
	S.i = 1

	S.stop = function ()
	end

	S.next = function (o, done)
		if S.i > table.maxn(S.list) then
			if not o.loop then
				if S.on_stop then S.on_stop() end
				return
			else
				S.i = 1
			end
		end
		local s = S.list[S.i]
		done({url=s})
		S.i = S.i + 1
	end

	return S
end

radio = R

