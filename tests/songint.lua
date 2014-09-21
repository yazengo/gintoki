
audio.play {
	url = 'testaudios/10s-1.mp3',
}

set_interval(function ()
	audio.play{ url = 'testaudios/2s-1.mp3' }
end, 1000)

