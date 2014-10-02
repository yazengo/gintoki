
require('cmd')

airplay_start('Muji')

airplay_on_start = function ()
	info('airplay starts')
	audio.play {
		url = 'airplay://',
	}
end

input.cmds = {
	[[ audio.play { url = 'testaudios/10s-1.mp3' } ]],
	[[ airplay_start('Hello1') ]],
	[[ airplay_start('Hello2') ]],
}

