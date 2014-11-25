
require('audio')

player = function (sink)
	local p = {}

	p.sink = sink

	p.stat = 'stopped' 
	p.paused = false

	-- fetching / fetching_paused 
	-- playing / playing_paused 
	-- buffering / buffering_paused
	-- changing_src
	-- stopped

	-- return {stat='buffering/playing/paused', url=..., dur=33}
	-- return {stat='fetching'}
	p.info = function ()
	end

	p.on_change = function ()
		if p.changed_cb then p.changed_cb() end
	end

	p.changed = function (cb)
		p.changed_cb = cb
	end

	p.pause = function ()
		if p.paused then return end
		p.paused = true

		if p.stat == 'playing' or p.stat == 'buffering' then
			p.copy.pause()
			p.on_change()
		elseif p.stat == 'fetching' then
			p.src.cancel_fetch()
			p.on_change()
		end
	end

	p.resume = function ()
		if not p.paused then return end
		p.paused = false

		if p.stat == 'playing' or p.stat == 'buffering' then
			p.copy.resume()
			p.on_change()
		elseif p.stat == 'fetching' then
			p.src.fetch(p.src_fetch)
			p.on_change()
		end
	end

	p.src_fetch = function (song)
		if not isstring(song.url) then
			panic('url must be set')
		end

		p.url = song.url
		p.dec = audio.decoder(p.url)

		p.stat = 'buffering'
		p.on_change()

		p.dec.probed(function (d)
			p.dur = d.dur
		end)

		local c = pipe.copy(p.dec, p.sink, 'bw')

		c.buffering(1500, function (buffering)
			if buffering and p.stat == 'playing' then
				p.stat = 'buffering'
				p.on_change()
			elseif not buffering and p.stat == 'buffering' then
				p.stat = 'playing'
				p.on_change()
			end
		end)
		
		c.done(function ()
			info('done')
			p.copy = nil
			p.stat = 'fetching'
			if not p.paused then
				p.src.fetch(p.src_fetch)
			end
		end)

		p.copy = c
	end

	p.src_skip = function ()
		if p.stat == 'playing' or p.stat == 'buffering' then 
			p.copy.close()
		end
	end

	p.setsrc = function (src)
		src.skip = p.src_skip

		if p.stat == 'playing' or p.stat == 'buffering' then
			p.stat = 'changing_src'
			p.copy.close()
		elseif p.stat == 'fetching' and not p.paused then
			p.src.cancel_fetch()
			src.fetch(p.src_fetch)
		elseif p.stat == 'stopped' then
			p.stat = 'fetching'
			src.fetch(p.src_fetch)
		end

		p.src = src
		return p
	end

	return p
end

