
require('peerctrl')
require('ctrl')
require('cmd')

ctrl.start()

input.cmds = {
	[[ peerctrl.on_msg {song={title='Mayonaise', album='Siamese Dream', artist='The Smashing Pumpkins', url='testaudios/10s-1.mp3'}} ]],
}

