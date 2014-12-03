
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

	r.rx = function ()
		return r.setopt('get.rx')
	end

	r.pause = function ()
		local b = r.setopt('pause')
		if b then
			set_immediate(function ()
				if r.statchanged_cb then
					r.statchanged_cb{stat='paused'}
				end
			end)
		end
		return b
	end

	r.resume = function ()
		local b = r.setopt('resume')
		set_immediate(function ()
			if r.statchanged_cb then
				r.statchanged_cb{stat='playing'}
			end
		end)
		return b
	end

	r.close = function ()
		r.setopt('close')
	end

	r.statchanged = function (cb)
		r.statchanged_cb = cb

		if not r.setopt('is_paused') then
			set_immediate(function ()
				cb{stat='buffering'}
			end)
			r.setopt('first_cb', function ()
				cb{stat='playing'}
			end)
		end

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

	local r = pexec(string.format('curl %s "%s"', o.timeout, o.url), 'rc')
	r[2].exit_cb = function (code)
		o.done(code)
	end
	return r[1]
end

pipe.filebuf = function (path)
end

