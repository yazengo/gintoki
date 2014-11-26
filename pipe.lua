
pipe = {}

pipe.copy = function (src, sink, mode)
	if src[1] then src = src[1] end
	if sink[1] then sink = sink[1] end

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

		bufing.first_cb = function ()
			bufing.rx = 0
			bufing.delta = 1
			bufing.timer = set_interval(bufing.check, timeout)
			bufing.cb(false)
			r.bufing = bufing
		end

		r.setopt('first_cb', bufing.first_cb)
		return r
	end

	return r
end

pipe.readall = function (p, done) 
	if p[1] then p = p[1] end
	local ss = strsink()

	ss.done_cb = function (str)
		done(str)
	end

	pipe.copy(p, ss)
end

pipe.grep = function (p, word, done)
	if p[1] then p = p[1] end
	local ss = strsink('grep', word)

	ss.grep_cb = function (str)
		done(str)
	end

	pipe.copy(p, ss)
end

