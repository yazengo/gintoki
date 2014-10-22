
require('audio')

audio.play { url = 'testaudios/10s-1.mp3' }

set_timeout(function ()
	audio.alert {
		url = 'testaudios/connected.mp3',
		vol = 80,
	}
end, 1000)


