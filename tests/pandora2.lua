
require('pandora2')
require('cmd')

pandora.test()

if input then
	input.cmds = {
		[[ pandora.test_login() ]],
		[[ info(pandora.info()) ]],
		[[ info(pandora.next()) ]],
		[[ pandora.setopt({op='pandora.login', username='enliest@qq.com', password='enliest1653'}, info) ]],
		[[ pandora.setopt({op='pandora.login', username='cfanfrank@gmail.com', password='enliest1653'}, info) ]],
		[[ pandora.setopt({op='pandora.login', username='fake', password='fake'}, info) ]],
		[[ pandora.setopt({op='pandora.genre_choose', id=argv[2]}, info) ]],
		[[ pandora.setopt({op='pandora.station_choose', id=argv[2]}, info) ]],
		[[ pandora.setopt({op='pandora.genres_list'}, info) ]],
		[[ pandora.setopt({op='pandora.stations_list'}, info) ]],
		[[ pandora.setopt({op='pandora.rate_like'}, info) ]],
		[[ pandora.setopt({op='pandora.rate_ban'}, info) ]],
	}
end


