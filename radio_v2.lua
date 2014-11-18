
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
-- typedef struct {
--   int type; // PTYPE_CALLBACK, PTYPE_FD, PTYPE_DIRECT
--   void (*trans)(pipe_t *p, void *buf, int size);
--   pipe_t *peer;
--   int fd;
--   void *data;
-- } pipe_t;
--
-- typedef struct {
-- } pipecopy_t;
--
-- void uv_pipecopy_start(pipecopy_t *cpy, pipe_t *src, pipe_t *sink) {
--   uv_splice(src, sink);
-- }
-- uv_pipecopy_stop(pipecopy_t *cpy);
--
-- [urlopen:1  -> file:??]
-- [adecoder:0 -> avconv:stdin]
--
-- int oldfd = open("app_log", O_RDWR|O_CREATE, 0644);
-- dup2(oldfd, 1);
-- close(oldfd);
--

adecoder = function (on_meta)
	local p = popen('avconv -i - -f s16le -ar 44100 -ac 2 -')
	avprobeparser(p.stderr, on_meta)
	return p.stdout
end

urlopen = function (url)
	if string.hasprefix(url, 'http://') then
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

http_server {
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

http_server {
	parse_headers = true,
	handler = function (req, res)
		apipe(req.body, fopen('file', 'w+')).closed(function ()
			res.send{}
		end)
	end,
}

tcp_server {
	host = 'localhost',
	port = 8811,
	handler = function (req, res)
		airplay.on_start(req.body, function ()
			res.close()
		end)
	end,
}

