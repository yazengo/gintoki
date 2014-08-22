
M = {}

emitter_init(M)

M.vol = 100
M.info = function () 
	return {
		battery = 90,
		volume = audio.get_vol(),
		wifi = {ssid="Sugr"},
		firmware_version = "1.0.1",
		firmware_need_update = true,
	}
end

M.set_vol = function (vol) 
	M.emit('stat_change')
	return audio.set_vol(vol)
end

muno = M

local R = {}

emitter_init(R)

R.playlist = localmusic

R.cursong = function ()
	return R.playlist.cur()
end

R.info = function ()
	local r = { type='pandora', station='POP' }
	return table.add(r, R.cursong() or {})
end

R.next = function ()
	info('radio next')
	R.playlist.next()
	R.emit('play', R.cursong())
end

R.prev = function ()
	info('radio prev')
	R.playlist.prev()
	R.emit('play', R.cursong())
end

radio = R

