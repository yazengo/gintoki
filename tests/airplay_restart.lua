
airplay_start('XX Muno')

airplay_on_start = function ()
	info('airplay starts')
	audio.play {
		url = 'airplay://',
	}
end

set_interval(function ()
	info('restart')
	airplay_start('XX Muno')
end, 300)

