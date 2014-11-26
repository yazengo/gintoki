
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

p2 = player.new().setstr(radio.urls_list{
	'testaudios/2s-1.mp3',
	'testaudios/2s-2.mp3',
	'testaudios/2s-3.mp3',
	loop=true,
})

mc = pipe.copy(p1, ao)

breakin = function (src)
	if c then
		c.close()
	end

	if type(src) == 'string' then src = audio.decoder(src) end
	mc.pause()
	c = pipe.copy(src, ao, 'b').done(function ()
		mc.resume()
	end)
end

input.cmds = {
	[[ breakin('testaudios/2s-1.mp3') ]],
	[[ breakin('testaudios/10s-1.mp3') ]],
	[[ breakin(p2) ]],
}

