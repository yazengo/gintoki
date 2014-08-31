
audio.play {
	track = 0,
	url = 'testaudios/10s-1.mp3'
}

set_timeout(function ()
	audio.play {
		track = 1,
		url = 'testaudios/hello-world.mp3'
	}
end, 500)


