
local R = {}

R.info = function ()
	local r = {}
	table.add(r, R.source.info())
	table.add(r, R.song or {})
	return r
end

R.source_setopt = function (opt, done) 
	if R.source.setopt then
		R.source.setopt(opt, done)
	end
end

R.next = function (opt)
	info('next')

	R.song = R.source.next(opt)
	if not R.song then return end
	if R.play then R.play(R.song) end
end

R.prev = function ()
	info('prev')

	R.song = R.source.prev()
	if not R.song then return end
	if R.play then R.play(R.song) end
end

R.start = function (source)
	info("start")
	R.source = source
	source.next_callback = function ()
		if R.source == source then R.next() end
	end
	if R.source.start then
		R.source.start()
	end
	R.next()
end

radio = R

