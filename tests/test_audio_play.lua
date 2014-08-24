
print('-- test audio mixer on done --')

i = 0
audio.play('testdata/10s-2.mp3', function () 
	print('ok')
end)

vol = 100

poll = function ()
	audio.setvol(vol)
	if vol == 80 then audio.pause() end
	if vol == 70 then audio.resume() end
	print(cjson.encode(audio.info()), audio.getvol())
	vol = vol - 10
	set_timeout(poll, 1000)
end

poll()

