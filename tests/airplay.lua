
setloglevel(0)

on_airplay_start = function ()
	info('airplay starts')
	audio.play {
		url = 'airplay://',
	}
end

