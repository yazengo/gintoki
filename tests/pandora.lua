
require('pandora')
require('cmd')

input.cmds = {
	[[ pandora.fetch(info) ]],
	[[ pandora.setopt({op='pandora.genres_list'}, info) ]],
	[[ pandora.setopt({op='pandora.stations_list'}, info) ]],
	[[ pandora.setopt({op='pandora.login', username='cfanfrank@gmail.com', password='enliest1653'}, info) ]],
	[[ pandora.setopt({op='pandora.login', username='WRONGEMAIL', password='WRONGPASSWORD'}, info) ]],
}

