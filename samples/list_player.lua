
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

p = player.new()
c = pipe.copy(p, audio.out(), 'br')

p.closed(function ()
	info('closed')
end)

p.changed(function (r)
	info(r)
end)

p.setsrc(s1)

input.cmds = {
	[[ s1.skip() ]],
	[[ s2.skip() ]],
	[[ s1.close() ]],
	[[ s2.close() ]],
	[[ p.setsrc(s1) ]],
	[[ p.setsrc(s2) ]],
	[[ p.pause() ]],
	[[ p.resume() ]],
	[[ c.close() ]],
	[[ info(p.pos()) ]],
}

