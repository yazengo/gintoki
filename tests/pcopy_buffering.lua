
require('pipe')
require('cmd')

src = asrc()
sink = aout()
src.setopt('pause')
pipe.copy(src, sink, 'b').buffering(1000, function (b)
	info('buffering', b)
end)

input.cmds = {
	[[ src.setopt('pause') ]],
	[[ src.setopt('resume') ]],
}

