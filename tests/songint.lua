
audio.play {
	url = 'testaudios/10s-1.mp3',
}

set_timeout(function ()
	audio.play{ url = 'testaudios/2s-1.mp3' }
end, 1000)

