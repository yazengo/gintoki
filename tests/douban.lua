
require('douban')
require('cmd')

input.cmds = {
	[[ douban.fetch(info) ]],
	[[ douban.setopt({op='douban.channels_list'}, info) ]],
	[[ douban.setopt({op='douban.login', username='enliest@qq.com', password='enliest1653'}, info) ]],
	[[ douban.setopt({op='douban.login', username='enliest@qq.com', password='WRONGPASSWORD'}, info) ]],
}

