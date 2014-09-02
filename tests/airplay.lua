
setloglevel(0)

airplay_start('xxx')

on_airplay_start = function ()
	info('airplay starts')
	audio.play {
		url = 'airplay://',
	}
end

