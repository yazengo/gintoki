
require('douban')
require('cmd')

douban.verbose = 1

douban.next_callback = function (r)
	info('next', douban.next())
end

douban.stop_callback = function (r)
	info('stop')
end

input.cmds = {
	[[ info(douban.next()) ]],
	[[ info('fetching', douban.is_fetching()) ]],
	[[ douban.setopt({op='douban.channels_list'}, info) ]],
	[[ douban.setopt({op='douban.login', username='enliest@qq.com', password='enliest1653'}, info) ]],
	[[ douban.setopt({op='douban.login', username='enliest@qq.com', password='WRONGPASSWORD'}, info) ]],
}

