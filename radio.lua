
local R = {}

R.log = function (...)
	info('radio:', ...)
end

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
	R.log('next')

	R.song = R.source.next(opt)
	if not R.song then return end
	if R.play then R.play(R.song) end
end

R.prev = function ()
	R.log('prev')

	R.song = R.source.prev()
	if not R.song then return end
	if R.play then R.play(R.song) end
end

R.start = function (source)
	R.log("start")
	if source.start then
		source.start()
	end
	R.source = source
	source.next_callback = function ()
		if R.source == source then R.next() end
	end
	R.next()
end

radio = R

