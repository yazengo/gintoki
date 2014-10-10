
-- test audio

print('-- audio test --')

audio.on('done', function () 
	info('audio done')
end)
audio.on('stat_change', function () 
	info(cjson.encode(audio.info()))
end)
audio.play{title='haha', duration=1}

if false then
	set_timeout(function () 
		audio.next()
	end, 1000)
end


