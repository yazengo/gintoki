
local R = {}

R.info = function ()
	local r = {}
	table.add(r, R.source.info())
	table.add(r, R.song or {})
	return r
end

R.setopt = function (opt, done) 
	if R.source and R.source.setopt then
		return R.source.setopt(opt, done)
	end
end

R.next = function (opt)
	info('next')

	if not R.source or not R.source.next then return end
	R.song = R.source.next(opt)
	if not R.song then return end
	if R.play then R.play(R.song) end
end

R.prev = function (opt)
	info('prev')

	if not R.source or not R.source.prev then return end
	R.song = R.source.prev(opt)
	if not R.song then return end
	if R.play then R.play(R.song) end
end

R.start = function (source)
	info("start")
	R.source = source

	source.next_callback = function ()
		if R.disabled then return end
		if R.source == source then R.next() end
	end

	source.stop_callback = function ()
		if R.disabled then return end
		if R.source == source then 
			if R.stop then R.stop() end 
		end
	end

	if R.source.start then
		R.source.start()
	end

	R.next()
end

radio = R

