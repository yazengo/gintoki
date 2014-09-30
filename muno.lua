
local M = {}

M.getinfo = function (done) 
	local ret = function (ssid)
		done {
			battery = 90,
			volume = audio.getvol(),
			wifi = {ssid = ssid or 'Unknown'},
			firmware_version = "1.0.1",
			name = hostname(),
			local_music_num = table.maxn(localmusic.list),
		}
	end
	if muno.getssid then
		muno.getssid(ret)
	else
		ret()
	end
end

M.notify_vol_change = function (vol)
	pnp.notify { ['muno.info'] = { volume = vol } }
end

M.firmwares = {}

M.check_update = function (done)
	curl {
		url = 'firmware.sugrsugr.com/info',
		done = function (ret)
			local r = cjson.decode(ret) or {}
			M.firmwares = r.firmwares_list or {}
			done(r)
		end,
	}
end

M.do_update = function (done)
	local first = M.firmwares[1]

	info('do_update starts')

	if not first then 
		done {
			result = 4,
			msg = 'not found',
		}	
		return
	end

	local url = first.url
	if not url then
		done {
			result = 5,
			msg = 'url invalid',
		}
		return
	end

	done {result = 0}

	local url_zip = url .. '.zip'
	local url_md5 = url .. '.md5'

	local path_zip = '/mnt/sdcard/update.zip'
	local path_md5 = '/mnt/sdcard/update.md5'
	--local path_zip = '/tmp/update.zip'
	--local path_md5 = '/tmp/update.md5'

	local curl_zip = function (ok, fail)
		system('curl "' .. url_zip .. '" > ' .. path_zip, function (r)
			if r == 0 then ok() else fail() end
		end)
	end

	local curl_md5 = function (ok, fail)
		system('curl "' .. url_md5 .. '" > ' .. path_md5, function (r)
			if r == 0 then ok() else fail() end
		end)
	end

	local check_md5 = function (ok, fail)
		system('md5sum -c ' .. path_md5, function (r)
			if r == 0 then ok() else fail() end
		end)
	end

	local downloading = function ()
		pnp.notify { ['muno.update_stat'] = {
			stat = 'downloading',
			progress = 0,
		}}
	end

	local download_failed = function ()
		pnp.notify { ['muno.update_stat'] = {
			stat = 'error',
			code = 11,
			msg = 'downloading failed',
		}}
	end

	local installing = function ()
		pnp.notify { ['muno.update_stat'] = {
			stat = 'installing',
			progress = 0,
		}}
	end

	local install_failed = function ()
		pnp.notify { ['muno.update_stat'] = {
			stat = 'error',
			code = 10,
			msg = 'install failed',
		}}
	end

	local complete = function ()
		pnp.notify { ['muno.update_stat'] = {
			stat = 'complete',
		}}
		system('reboot_recovery')
	end

	downloading()
	curl_zip(function ()
		curl_md5(function ()
			installing()
			check_md5(complete, install_failed)
		end, download_failed)
	end, download_failed)

end

muno = M


