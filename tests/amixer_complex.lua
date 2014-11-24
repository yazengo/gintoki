
require('audio')
require('cmd')

function dec(i)
	return audio.decoder(string.format('testaudios/2s-%d.mp3', i))
end

am = audio.mixer()
am.add(dec(1))

am2 = audio.mixer()
am2.add(dec(2))
am2.add(am)

am3 = audio.mixer()
am3.add(am2)
am3.add(dec(3))

audio.pipe(am3, audio.out()).done(function ()
	info('done')
end)

input.cmds = {
	[[ ]],
}

