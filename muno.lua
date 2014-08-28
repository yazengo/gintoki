
local M = {}

emitter_init(M)

M.vol = 100
M.info = function () 
	return {
		battery = 90,
		volume = audio.getvol(),
		wifi = {ssid="Sugr"},
		firmware_version = "1.0.1",
		firmware_need_update = true,
		name = 'K.B.Z',
		local_music_num = table.maxn(localmusic.list),
	}
end

M.setvol = function (vol) 
	M.emit('stat_change')
	return audio.setvol(vol)
end

muno = M


