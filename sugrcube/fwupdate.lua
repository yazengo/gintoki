
local F = {}

F.cancel = function ()
	if F.curl then F.curl.cancel() end
end

F.start = function (o, notify, finish)

	local quit = function ()
		info('fwupdate quit')
		F.curl = nil
	end

	info('fwupdate starts')

	local recovery = 	function ()
		set_timeout(function ()
			info('fwupdate complete')
			finish()
		end, 2000)
	end

	local path_zip = os.getenv('FWUPDATEZIP') or 'update.zip'

	local downloading = function (p)
		notify {
			stat = 'downloading',
			progress = p,
		}
	end

	local curl_zip = function (ok, fail)
		local timer

		F.curl = curl {
			url = fw.url,
			retfile = path_zip,
			done = function (r, st)
				clear_interval(timer)
				F.curl = nil
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
			local st = F.curl:stat()
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
		notify {
			stat = 'error',
			code = 11,
			msg = 'downloading failed',
		}
		quit()
	end

	local md5sum_failed = function ()
		notify {
			stat = 'error',
			code = 10,
			msg = 'md5sum failed',
		}
		quit()
	end

	local installing = function ()
		notify {
			stat = 'installing',
			progress = 0,
		}
		quit()
		recovery()
	end

	curl_zip(function ()
		check_md5(installing, md5sum_failed)
	end, download_failed)

end

fwupdate = F

