
require('pipe')
require('audio')
require('cmd')

c = pipe.copy(asrc('noise'), audio.out())

input.cmds = {
	[[ c.setopt('pause') ]],
	[[ c.setopt('resume') ]],
}

