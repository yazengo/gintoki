
require('prop')

local F = {}

F.setopt = function (a, done)
	if a.op == 'muno.check_update' then
		F.check_update(done)
		return true
	elseif a.op == 'muno.do_update' then
		F.do_update(done)
		return true
	elseif a.op == 'muno.cancel_update' then
		F.cancel_update(done)
		return true
	elseif a.op == 'muno.set_update_url' then
		prop.set('fwupdate.url', a.url)
		done{result=0}
		return true
	end
end

F.firmwares = {}

F.check_update = function (done)
	local url = prop.get('fwupdate.url', 'firmware.sugrsugr.com/info')
	curl {
		url = url,
		done = function (ret)
			local r = cjson.decode(ret) or {}
			F.firmwares = r.firmwares_list or {}
			done(r)
		end,
	}
end

F.cancel_update = function (done)
	if F.hcurl then
		F.hcurl.cancel()
		done{result=0}
	else
		done{result=1}
	end
end

F.do_update = function (done)
	local fw = F.firmwares[1]

	if F.updating then
		done{result=1, msg='already updating'}
		return
	end

	F.updating = true
	local quit = function ()
		F.updating = false
	end

	info('do_update starts')

	if not fw then 
		done{result=1, msg='do check_update first'}	
		return
	end

	local root = fwupdate_root or ''
	local path_zip = root .. 'update.zip'

	local downloading = function (p)
		F.notify  {
			stat = 'downloading',
			progress = p,
		}
	end

	local curl_zip = function (ok, fail)
		local timer

		F.hcurl = curl {
			url = fw.url,
			retfile = path_zip,
			done = function (r, st)
				clear_interval(timer)
				F.hcurl = nil
				info(st)
				if st.stat == 'cancelled' then
					quit()
					return
				end
				if st.stat == 'done' then
					ok()
				else
					fail()
				end
			end,
		}

		downloading(0)
		timer = set_interval(function ()
			local st = F.hcurl.stat()
			downloading(math.ceil(st.progress*100))
		end, 1000)
	end

	local md5sum = function (file, done, fail)
		popen {
			cmd = 'md5sum ' .. file, 
			done = function (r, code)
				if #r <= 32 then
					fail()
					return
				end
				done(string.sub(r, 1, 32))
			end
		}
	end

	local check_md5 = function (ok, fail)
		md5sum(path_zip, function (md5)
			if md5 == fw.md5 then
				ok()
			else
				fail()
			end
		end, fail)
	end

	local download_failed = function ()
		F.notify {
			stat = 'error',
			code = 11,
			msg = 'downloading failed',
		}
		quit()
	end

	local md5sum_failed = function ()
		F.notify {
			stat = 'error',
			code = 10,
			msg = 'md5sum failed',
		}
		quit()
	end

	local installing = function ()
		F.notify {
			stat = 'installing',
			progress = 0,
		}
		quit()
		fwupdate_recovery()
	end

	curl_zip(function ()
		check_md5(installing, md5sum_failed)
	end, download_failed)

	done{result=0}
end

fwupdate = F

