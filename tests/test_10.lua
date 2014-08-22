
print('-- test audio mixer on done --')

i = 0
audio.play('testdata/10s-2.mp3', function () 
	print('ok')
end)

poll = function ()
	print(cjson.encode(audio.info()))
	set_timeout(poll, 1000)
end

set_timeout(poll, 1000)

