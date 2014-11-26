
require('cmd')
require('audio')

ao = audio.out()
noise = audio.noise()

change = function (src, close)
	if type(src) == 'string' then
		src = audio.decoder(src)
	end
	if c then 
		c.close()
	end
	c = pipe.copy(src, ao, 'b' .. (close or ''))
end

input.cmds = {
	[[ change('testaudios/2s-1.mp3', 'r') ]],
	[[ change('testaudios/2s-2.mp3', 'r') ]],
	[[ change(noise) ]],
}

