
print('-- radio & audio test --')

audio.on('stat_change', function ()
	info('audio: ' .. cjson.encode(audio.info()))
end)

radio.on('play', function () 
	info(cjson.encode(radio.info()))
	audio.play(radio.cursong())
end)

audio.on('done', function ()
	radio.next()
end)

audio.play(radio.cursong())

set_timeout(function ()
	info('next')
	radio.next()
end, 2400)

