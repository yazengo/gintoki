
require('audio')
require('radio')
require('pipe')
require('player')
require('cmd')

ao = audio.out()

p1 = player.new().setsrc(radio.urls_list{
	'testaudios/10s-1.mp3',
	'testaudios/10s-2.mp3',
	'testaudios/10s-3.mp3',
	loop=true,
})

p2 = player.new().setsrc(radio.urls_list{
	'testaudios/2s-2.mp3',
	'testaudios/2s-3.mp3',
})

c = pipe.copy(p1, ao, '')
n = 0

breakin = function (src)
	if c then
		c.close()
	end

	if type(src) == 'string' then src = audio.decoder(src) end
	n = n + 1

	c = pipe.copy(src, ao, 'r').done(function ()
		n = n - 1
		if n == 0 then
			c = pipe.copy(p1, ao, '')
		end
	end)
end

input.cmds = {
	[[ breakin('testaudios/2s-1.mp3') ]],
	[[ breakin('testaudios/10s-2.mp3') ]],
	[[ if p2 then breakin(p2); p2 = nil end ]],
}

