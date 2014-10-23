
require('cmd')

o = oncer()
o.done = function ()
	info('done')
end

input.cmds = {
	[[ o:update() ]],
}

