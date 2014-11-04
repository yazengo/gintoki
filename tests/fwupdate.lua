
require('fwupdate')
require('cmd')

fwupdate.notify = info
fwupdate_recovery = function ()
	info('recovery starts')
end

input.cmds = {
	[[ fwupdate.setopt({op='muno.check_update'}, info) ]],
	[[ fwupdate.setopt({op='muno.do_update'}, info) ]],
	[[ fwupdate.setopt({op='muno.cancel_update'}, info) ]],
	[[ fwupdate.setopt({op='muno.set_update_url', url='local-firmware.sugrsugr.com/info'}, info) ]],
	[[ fwupdate.setopt({op='muno.set_update_url', url='firmware.sugrsugr.com/info'}, info) ]],
}

