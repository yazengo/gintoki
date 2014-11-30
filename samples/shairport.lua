
require('shairport')
require('audio')
require('pipe')
require('cmd')

ao = audio.out()

shairport.start(function (r)
	c = pipe.copy(r, ao, 'br').done(function ()
		c = nil
		info('ends')
	end)
end)

input.cmds = {
	[[ if c then c.close() end ]],
}

