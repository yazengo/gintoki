
require('fwupdate')
require('cmd')

fwupdate.curversion = function ()
	return 'NightlyBuild-' .. BUILDDATE
end
fwupdate.notify = info
fwupdate_recovery = function ()
	info('recovery starts')
end

info(fwupdate.curversion())

input.cmds = {
	[[ fwupdate.curversion = function () return 'NightlyBuild-20150101-1100' end ]],
	[[ fwupdate.curversion = function () return 'NightlyBuild-20130101-1100' end ]],
	[[ fwupdate.setopt({op='muno.check_update'}, info) ]],
	[[ fwupdate.setopt({op='muno.do_update'}, info) ]],
	[[ fwupdate.setopt({op='muno.cancel_update'}, info) ]],
	[[ fwupdate.setopt({op='muno.set_update_url', url='local-firmware.sugrsugr.com/info'}, info) ]],
	[[ fwupdate.setopt({op='muno.set_update_url', url='firmware.sugrsugr.com/info'}, info) ]],
}

