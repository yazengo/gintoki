
print('-- test audio mixer --')

i = 0
play = function ()
	audio.play('testdata/10s-' .. (i+1) .. '.mp3')
	i = (i + 1) % 3
	set_timeout(play, 2000)
end

play()

