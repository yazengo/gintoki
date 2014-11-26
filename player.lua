
require('audio')

player = {}

player.new = function ()
	local p = pdirect()

	p.stat = 'stopped' 
	p.paused = false

	-- fetching
	-- playing
	-- buffering
	-- changing_src
	-- stopped
	-- closed

	-- return {stat='buffering/playing/paused', url=..., dur=33}
	-- return {stat='fetching'}
	p.info = function ()
	end

	p.pos = function ()
		if p.stat == 'playing' or p.stat == 'buffering' then
			return p.copy.rx() / (44100*4)
		end
	end

	p.on_change = function ()
		local r = {stat=p.stat}
		if p.paused and r.stat == 'playing' then r.stat = 'paused' end
		set_immediate(function ()
			if r.stat == 'closed' and p.closed_cb then p.closed_cb() end
			if p.changed_cb then p.changed_cb(r) end
		end)
	end

	p.closed = function (cb)
		p.closed_cb = cb
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

		local c = pipe.copy(p.dec, p, 'br')

		c.buffering(1500, function (buffering)
			if buffering and p.stat == 'playing' then
				p.stat = 'buffering'
				if not p.paused then p.on_change() end
			elseif not buffering and p.stat == 'buffering' then
				p.stat = 'playing'
				if not p.paused then p.on_change() end
			end
		end)
		
		c.done(function (reason)
			info('done', 'stat=', p.stat, 'reason=', reason)
			p.copy = nil

			if reason == 'w' then
				-- EOF
				pclose_write(p)
				p.stat = 'closed'
				p.on_change()
				return
			end

			if p.stat == 'playing' or p.stat == 'buffering' or p.stat == 'changing_src' then
				p.stat = 'fetching'
				p.on_change()
				if not p.paused then p.src.fetch(p.src_fetch) end
			elseif p.stat == 'closing' then
				p.stat = 'closed'
				p.on_change()
			end
		end)

		p.copy = c
	end

	p.src_skip = function ()
		if p.stat == 'playing' or p.stat == 'buffering' then 
			p.copy.close()
		end
	end

	p.src_close = function ()
		p.src.close = nil
		p.src.skip = nil

		if p.stat == 'playing' or p.stat == 'buffering' then
			p.copy.close()
			p.stat = 'closing'
		elseif p.stat == 'fetching' then
			p.src.cancel_fetch()
			p.stat = 'closed'
			p.on_change()
		end

		p.src = nil
	end

	p.setsrc = function (src)
		src.skip = p.src_skip
		src.close = p.src_close

		if p.stat == 'playing' or p.stat == 'buffering' then
			p.stat = 'changing_src'
			p.copy.close()
		elseif p.stat == 'fetching' and not p.paused then
			p.src.cancel_fetch()
			src.fetch(p.src_fetch)
		elseif p.stat == 'stopped' then
			p.stat = 'fetching'
			src.fetch(p.src_fetch)
			p.on_change()
		elseif p.stat == 'closed' then
			panic('aleady closed')
		end

		p.src = src
		return p
	end

	return p
end

