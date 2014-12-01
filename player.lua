
require('audio')

player = {}

player.new = function ()
	local p = pdirect()

	p.pos = function ()
		if p.stat == 'playing' or p.stat == 'buffering' then
			return p.copy.rx() / (44100*4)
		end
	end

	p.src_fetch = function (song)
		if not isstring(song.url) then
			panic('url must be set')
		end

		p.song = song
		p.dec = audio.decoder(song.url)

		p.dec.probed(function (d)
			p.dur = d.dur
		end)

		local c = pipe.copy(p.dec, p, 'br')
		
		c.done(function (reason)
			p.song = nil
			p.copy = nil
			if reason == 'r' then
				p.src.fetch(p.src_fetch)
			elseif reason == 'w' then
				pclose_write(p)
			elseif reason == 'c' then
				-- closed by p.setsrc
			end
		end)

		p.copy = c
	end

	p.src_skip = function ()
		if p.copy then p.copy.close() end
	end

	p.setsrc = function (src)
		if p.src then p.src.cancel_fetch() end
		if p.copy then p.copy.close() end
		p.src = src
		p.src.fetch(p.src_fetch)
		return p
	end

	return p
end

