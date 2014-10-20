
require('douban')
require('cmd')

--P.cookie.station_id = '2178401799297707427'

if input then
	input.cmds = {
		[[ info(douban.info()) ]],
		[[ info(douban.next()) ]],
		[[ douban.user_login({username='enliest@qq.com', password='wrong'}, info) ]],
		[[ douban.setopt{op='douban.login', username='enliest@qq.com', password='enliest1653'} ]],
		[[ douban.setopt{op='douban.login', username='sugr@sugrsugr.com', password='Sugr140331'} ]],
		[[ douban.setopt({op='douban.channels_list'}, info) ]],
		[[ douban.setopt({op='douban.channel_choose', id=argv[1]}, info) ]],
	}
end

