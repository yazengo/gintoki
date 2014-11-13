
require('luv');

aout

pcopy(aout, mixer)

p.pause()
p.resume()

m = amixer()
p = m.add()
m.del(p)
m.has(p)

m.setvol(13)
m.getvol() -- 1

m.highlight(p)

ctrl.mixer = amixer()
ctrl.psong = pipe()
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

pipe(popen('ls'), popen('cat >ls.log'))

pipe(fopen('a.mp3'), popen('avconv -i - -f s16le -ar 44100 -ac 2 -'), aout).closed(function ()
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

ctrl.playsong = function ()
	ctrl.psong = apipe(opener(ctrl.song.url), adecoder(), afilter{fadein=300}).closed(function ()
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

servermsg.on_msg = function (p)
	ctrl.insert(p, function ()
	end)
end

serversync.on_start = function (p)
	ctrl.insert(p, function ()
	end)
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

