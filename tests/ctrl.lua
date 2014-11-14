
require('ctrl')
require('radio')
require('cmd')

ctrl.start()

input.cmds = {
	[[ radio.setopt({op='audio.prev'}, info) ]],
	[[ radio.setopt({op='audio.next'}, info) ]],
	[[ ctrl.setopt({op='radio.change_type', type='douban'}, info) ]],
	[[ ctrl.setopt({op='radio.change_type', type='pandora'}, info) ]],
	[[ ctrl.setopt({op='radio.change_type', type='bbcradio'}, info) ]],
	[[ ctrl.setopt({op='audio.play_pause_toggle'}, info) ]],
	[[ ctrl.setopt({op='audio.next'}, info) ]],
	[[ ctrl.setopt({op='breaking.audio', url='testaudios/connected.mp3'}, info) ]],
	[[ ctrl.setopt({op='breaking.audio', url='testaudios/connected.mp3', resume=true}, info) ]],
}

