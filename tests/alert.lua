
require('audio')

audio.play { url = 'testaudios/10s-1.mp3' }

set_timeout(function ()
	audio.alert('testaudios/connected.mp3')
end, 1000)


