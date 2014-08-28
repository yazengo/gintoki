

setloglevel(0)

playnext = function ()
	audio.play({
		url = 'testdata/2s-1.mp3',
		done = playnext,
	})
end

playnext()

