
require('audio')
require('cmd')

audio.play { url = 'testaudios/10s-1.mp3' }

input.cmds = {
	[[ audio.alert {url='testaudios/connected.mp3', vol=80, fadeothers=false} ]],
	[[ audio.alert {url='testaudios/connected.mp3', vol=20, fadeothers=true} ]],
}

