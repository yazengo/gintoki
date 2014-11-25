
require('audio')
require('pipe')

playlist = function (tr, sink)
	local p = {}

	p.sink = sink
	p.tr = tr

	p.stat = 'stopped'

	-- return {stat='buffering/playing/paused', url=..., dur=33}
	-- return {stat='fetching'}
	p.info = function ()
	end

	p.on_change = function ()
	end

	p.pause = function ()
		tr.pause()
	end

	p.resume = function ()
		tr.resume()
	end

	p.src_skipped = function ()
	end

	p.src_next = function (song)
		if not isstring(song.url) then
			src.next(p.src_next)
			return
		end

		p.url = song.url
		p.stat = 'buffering'
		p.dec = audio.decoder(p.url)

		local c = pipe.copy(p.dec[1], p.sink, 'bw')

		c.buffering(1500, function (b)
			p.buffering = b
		end)
		
		c.done(function ()
		end)
	end

	p.setsrc = function (src)
		if p.stat == 'stopped' then
			p.stat = 'fetching'
		end
		src.skipped(p.src_skipped)
		src.next(p.src_next)
		return p
	end

	return p
end

playseq = function (...)
	local tr, tr_p = am.add()
	local urls = {...}
	local i = 1

	local function loop()
		local url = urls[i]
		i = i + 1
		if not url then return end
		local dec = audio.decoder(url)

		local p = pipe.copy(dec[1], tr_p, 'bw').done(function ()
			loop()
		end)
		set_timeout(function ()
			p.close()
		end, 1000)
	end

	loop()
end


radio.src()

m = audio.mixer()

m.add(audio.decoder()).done(function ()
end)


