
require('audio')

player = function (sink)
	local p = {}

	p.sink = sink

	-- return {stat='buffering/playing/paused', url=..., dur=33}
	-- return {stat='fetching'}
	p.info = function ()
	end

	p.on_change = function (st)
		set_immediate(function ()
			info(st)
		end)
	end

	p.pause = function ()
		if p.copy and p.copy.pause() then
			p.on_change{stat='paused'}
		end
		p.paused = true
	end

	p.resume = function ()
		p.paused = false
		if p.copy and p.copy.resume() then
			local stat = (p.buffering and 'buffering') or 'playing'
			p.on_change{stat=stat}
		end
	end

	p.src_fetch = function (song)
		p.fetching = false

		if not isstring(song.url) then
			panic('url must be set')
		end

		p.url = song.url
		p.dec = audio.decoder(p.url)

		p.on_change{stat='buffering'}

		p.dec.probed(function (d)
			p.on_change{stat='probed'}
		end)

		local c = pipe.copy(p.dec, p.sink, 'bw')

		c.buffering(1500, function (b)
			if p.paused then return end
			p.buffering = p
			if b then
				p.on_change{stat='buffering'}
			else
				p.on_change{stat='playing'}
			end
		end)
		
		c.done(function ()
			p.on_change{stat='fetching'}
			p.copy = nil
			p.fetching = true
			p.src.fetch(p.src_fetch)
		end)

		p.copy = c
	end

	p.src_skip = function ()
		if p.copy then p.copy.close() end
	end

	p.setsrc = function (src)
		if p.stat == 'stopped' then
			p.stat = 'fetching'
		end

		src.skip = p.src_skip
		src.stop = p.src_stop

		p.on_change{stat='fetching'}
		p.src = src
		p.src.fetch(p.src_fetch)

		return p
	end

	return p
end

