
require('poweroff')
require('fwupdate')

local M = {}

fwupdate.curversion = M.version

fwupdate.notify = function (r)
	pnp.notify { ['muno.update_stat'] = r }
end

M.setopt = function (a, done)
	if a.op == 'muno.request_sync' then
		pnp.notify_sync{['audio.info']=M.audioinfo()}
		done{result=0}
		return true
	elseif a.op == 'muno.request_event' then
		M.allinfo(function (r)
			pnp.notify_event(r)
		end)
		done{result=0}
		return true
	elseif a.op == 'muno.info' then
		M.info(function (r)
			done(r)
		end)
		return true
	elseif a.op == 'muno.set_poweroff_timeout' then
		return M.set_poweroff(a, done)
	elseif a.op == 'muno.cancel_poweroff_timeout' then
		return M.cancel_poweroff(a, done)
	elseif fwupdate.setopt(a, done) then
		return true
	end
end

M.set_poweroff = function (a, done)
	local timeout = tonumber(a.timeout)
	if not timeout then
		return
	end

	if M.poweroff then
		M.poweroff:cancel()
	end

	info('poweroff in', timeout, 's')

	M.poweroff = poweroff {
		timeout = timeout,
		notify = function (r)
			pnp.notify{['muno.info'] = {poweroff = r}}
		end,
		done = M.on_poweroff,
	}

	done{result=0}
	return true
end

M.cancel_poweroff = function (a, done)
	if M.poweroff then
		M.poweroff:cancel()
		M.poweroff = nil
	end

	done{result=0}
	return true
end

M.on_poweroff = function ()
	info('power off now')
	if arch_poweroff then arch_poweroff() end
	M.poweroff = nil

	handle{op='audio.pause', sleeping=true}
end

M.allinfo = function (done)
	M.info(function (r)
		done {
			['audio.info'] = M.audioinfo(),
			['muno.info'] = r,
		}
	end)
end

M.version = function ()
	if arch_version then
		return arch_version.revision
	else
		return BUILDDATE
	end
end

M.info = function (done) 

	local ret = function (ssid, ipaddr)
		done {
			battery = 90,
			volume = audio.getvol(),
			wifi = {ssid = ssid or 'Unknown'},
			itunes_server = {ipaddr = ipaddr or "Unknown", port = '8883'},
			firmware_version = M.version(),
			name = hostname(),
			local_music_num = table.maxn(localmusic.list),
			poweroff = M.poweroff and M.poweroff:info()
		}
	end

	if getssid then
		getssid(ret)
	else
		ret()
	end
end

M.notify_vol_change = function (vol)
	pnp.notify { ['muno.info'] = { volume = vol } }
end

muno = M

