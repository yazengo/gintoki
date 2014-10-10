
airplay_start('Muji')

airplay_on_start = function ()
	info('airplay starts')
	audio.play {
		url = 'airplay://',
	}
end

