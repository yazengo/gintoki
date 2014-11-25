
require('player')
require('audio')
require('radio')
require('cmd')

s1 = radio.urls_list {
	'testaudios/2s-1.mp3',
	'testaudios/2s-2.mp3',
	'testaudios/2s-3.mp3',
	loop = true,
}

s2 = radio.urls_list {
	'testaudios/10s-1.mp3',
	'testaudios/10s-2.mp3',
	'testaudios/10s-3.mp3',
	loop = true,
}

p = player(audio.out())
p.changed(function ()
	info(p.stat)
end)
p.setsrc(s1)

input.cmds = {
	[[ s1.skip() ]],
	[[ s2.skip() ]],
	[[ p.setsrc(s1) ]],
	[[ p.setsrc(s2) ]],
	[[ p.pause() ]],
	[[ p.resume() ]],
}

