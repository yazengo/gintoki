
require('cmd')

audio.setvol(30)
audio.play {
	url = 'noise://',
}

input.cmds = {
	[[ noise_setopt{mode='white'} ]],
	[[ noise_setopt{mode='pink'} ]],
}

