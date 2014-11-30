
pipe = {}

pipe.copy = function (src, sink, mode)
	if src[1] then src = src[1] end
	if sink[1] then sink = sink[1] end

	if not mode then panic('mode must be set') end

	local r = pcopy(src, sink, mode)

	r.done = function (cb)
		r.done_cb = function (...)
			if r.bufing then clear_interval(r.bufing.timer) end
			cb(...)
		end
		return r
	end 

	r.first_cb = function ()
		if r.start_first_cb then r.start_first_cb() end
		if r.bufing_first_cb then r.bufing_first_cb() end
	end

	r.started = function (cb)
		r.start_first_cb = cb
		r.setopt('first_cb', r.first_cb)
		return r
	end

	r.rx = function ()
		return r.setopt('get.rx')
	end

	r.pause = function ()
		return r.setopt('pause')
	end

	r.resume = function ()
		return r.setopt('resume')
	end

	r.close = function ()
		r.setopt('close')
	end

	r.buffering = function (timeout, cb)
		local bufing = {}

		bufing.check = function ()
			local rx = r.rx()
			local delta = rx - bufing.rx

			if delta == 0 and bufing.delta > 0 then
				bufing.cb(true)
			elseif delta > 0 and bufing.delta == 0 then
				bufing.cb(false)
			end

			--info('rx=', rx, bufing.rx, 'delta=', delta, bufing.delta)

			bufing.rx = rx
			bufing.delta = delta
		end

		bufing.cb = cb

		r.bufing_first_cb = function ()
			bufing.rx = 0
			bufing.delta = 1
			bufing.timer = set_interval(bufing.check, timeout)
			bufing.cb(false)
			r.bufing = bufing
		end

		r.setopt('first_cb', r.first_cb)
		return r
	end

	return r
end

pipe.readall = function (p, done) 
	if p[1] then p = p[1] end
	local ss = pstrsink()

	ss.done_cb = function (str)
		done(str)
	end

	pipe.copy(p, ss, 'rw')
end

pipe.grep = function (p, word, done)
	if p[1] then p = p[1] end
	local ss = pstrsink('grep', word)

	ss.grep_cb = function (str)
		done(str)
	end

	pipe.copy(p, ss, 'rw')
end

pipe.curl = function (...)
	local o = {...}

	o.url = o.url or o[1]
	o.done = o.done or o[2]

	o.done = o.done or function () end
	o.timeout = (o.timeout and '-m ' .. o.timeout) or ''

	local r = pexec(string.format('curl %s %s', o.timeout, o.url), 'rc')
	r[2].exit_cb = function (code)
		o.done(code)
	end
	return r[1]
end

pipe.filebuf = function (path)
end

