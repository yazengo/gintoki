
local mtime 
local get

get = function (done) 
	curl {
		url = 'push.sugrsugr.com:8880/sub/111',
		headers = {
			['If-Modified-Since'] = mtime,
		},
		done = function (r, st)
			info(r)
			get(done)
		end,
		on_header = function (k, v)
			if k == 'Last-Modified' then
				mtime = v
			end
		end,
	}
end

get()

