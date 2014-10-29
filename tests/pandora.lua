
require('pandora')
require('cmd')

pandora.verbose = 1

pandora.next_callback = function (r)
	info('next', pandora.next())
end

pandora.stop_callback = function (r)
	info('stop')
end

input.cmds = {
	[[ info(pandora.next()) ]],
	[[ info('fetching', pandora.is_fetching()) ]],
	[[ pandora.setopt({op='pandora.genres_list'}, info) ]],
	[[ pandora.setopt({op='pandora.stations_list'}, info) ]],
	[[ pandora.setopt({op='pandora.login', username='cfanfrank@gmail.com', password='enliest1653'}, info) ]],
	[[ pandora.setopt({op='pandora.login', username='WRONGEMAIL', password='WRONGPASSWORD'}, info) ]],
}

