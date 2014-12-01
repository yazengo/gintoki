
require('cmd')
require('audio')
require('pipe')

ae = audio.effect()

audio.pipe(audio.noise(), ae, audio.out())

input.cmds = {
	[[ ae.setvol(0.2) ]],
	[[ ae.setvol(0.5) ]],
	[[ ae.setvol(0.9) ]],
	[[ ae.fadein(1000) ]],
	[[ ae.fadeout(1000) ]],
	[[ ae.cancel_fade() ]],
}

