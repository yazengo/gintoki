
local F = {}

F.setopt = function (a, done)
	if a.op == 'muno.check_update' then
		F.check_update(done)
		return true
	elseif a.op == 'muno.do_update' then
		F.do_update(done)
		return true
	end
end

F.firmwares = {}

F.check_update = function (done)
	curl {
		url = 'firmware.sugrsugr.com/info',
		done = function (ret)
			local r = cjson.decode(ret) or {}
			F.firmwares = r.firmwares_list or {}
			done(r)
		end,
	}
end

F.do_update = function (done)
	local first = F.firmwares[1]

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

	local system = function (cmd, done)
		done = done or function () end
		popen {
			cmd = cmd,
			done = function (r, code) 
				done(code)
			end,
		}
	end

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

fwupdate = F

