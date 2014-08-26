
local R = {}

R.log = function (...)
	info('radio:', ...)
end

R.cursong = function ()
	return R.source.cursong()
end

R.info = function ()
	local r = {}
	table.add(r, R.source.info())
	table.add(r, R.cursong() or {})
	return r
end

R.source_setopt = function (opt, done) 
	if R.source.setopt then
		R.source.setopt(opt, done)
	end
end

R.next = function ()
	R.log('next')

	local song = R.source.next()
	if not song then return end
	if R.play then R.play(song) end
end

R.start = function (source)
	R.source = source
	source.next_callback = function ()
		if R.source == source then R.next() end
	end
	R.next()
end

radio = R

