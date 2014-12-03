
require('sugrcube/poweroff')
require('sugrcube/fwupdate')

local M = {}

-- need:
-- M.audio_info()
-- M.getvol()
-- M.localmusic_nr()

M.setopt_sync = function (o, done)
	pnp.notify_sync { ['audio.info'] = M.audio_info() }
	done()
end

M.setopt_event = function (o, done)
	M.allinfo(function (r) pnp.notify_event(r) end)
	done()
end

M.setopt_info = function (o, done)
	M.info(function (r) done(r) end)
end

M.setopt_poweroff = function (o, done)
	if type(o.timeout) ~= 'number' then
		done(1, 'invalid timeout')
		return
	end

	poweroff.start(o.timeout * 1000, function (r)
		pnp.notify { ['muno.info'] = { poweroff = r}}
	end)

	done()
end

M.setopt_cancel_poweroff = function (o, done)
	poweroff.cancel()
	done()
end

M.setopt_do_update = function (o, done)
	fwupdate.start(o, function (r)
		pnp.notify { ['muno.update_stat'] = r }
	end, function ()
		pnp.stop()
		arch.fwupdate()
	end)
	done()
end

M.setopt_cancel_update = function (o, done)
	fwupdate.cancel()
	done()
end

M.allinfo = function (done)
	M.info(function (r)
		done {
			['audio.info'] = M.audio_info(),
			['muno.info'] = r,
		}
	end)
end

M.version = function ()
	if arch.version then
		return arch.version()
	end
	return BUILDDATE
end

M.info = function (done) 

	local ret = function (r)
		done {
			battery = (arch.battery and arch.battery()) or 50,
			volume = M.getvol(),
			wifi = {ssid = r.ssid or '?'},
			itunes_server = {ipaddr = r.ipaddr or '?', port = '8883'},
			firmware_version = M.version(),
			name = hostname(),
			local_music_num = M.localmusic_nr(),
			poweroff = M.poweroff and M.poweroff.info()
		}
	end

	if arch.wifi_info then
		arch.wifi_info(ret)
	else
		ret({})
	end
end

M.notify_vol_change = function (vol)
	pnp.notify { ['muno.info'] = { volume = vol } }
end

muno = M

