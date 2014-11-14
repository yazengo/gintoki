
require('radio')
require('cmd')


input.cmds = {
	[[ radio.change{type='douban'} ]],
	[[ radio.change{type='local'} ]],
	[[ radio.next({}, info) ]],
}

