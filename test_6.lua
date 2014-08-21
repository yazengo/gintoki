
print('-- audio & radio test 2 --')

muno.on('stat_change', function () 
	info('muno stat change: ' .. cjson.encode(muno.info()))
end)

audio.on('stat_change', function ()
	local r = table.add(audio.info(), radio.info())
	info('audio stat change: ' .. cjson.encode(r))
end)

radio.on('play', function () 
	info(cjson.encode(radio.info()))
	audio.play(radio.cursong())
end)

audio.on('done', function ()
	radio.next()
end)

audio.play(radio.cursong())

