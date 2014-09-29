
playnext = function ()
	audio.play({
		url = 'testaudios/2s-1.mp3',
		done = playnext,
	})
end

playnext()

