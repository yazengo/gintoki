
local R = {}

emitter_init(R)

R.playlist = localmusic

R.cursong = function ()
	return R.playlist.cur()
end

R.info = function ()
	local r = { type='local', station='POP' }
	return table.add(r, R.cursong() or {})
end

R.next = function ()
	info('radio next')
	local song = R.playlist.next()
	if song then R.emit('play', song) end
end

R.prev = function ()
	info('radio prev')
	local song = R.playlist.prev()
	if song then R.emit('play', song) end
end

R.start = function ()
	R.next()
end

radio = R

