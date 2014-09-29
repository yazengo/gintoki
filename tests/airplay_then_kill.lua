
airplay_start('Muji')

airplay_on_start = function ()
	info('airplay starts')
	audio.play {
		url = 'airplay://',
	}
	set_timeout(function ()
		audio.stop()
	end, 4000)
end

