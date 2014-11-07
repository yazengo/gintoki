
require('prop')

local mtime = prop.get('comet.mtime')
local get

get = function (done) 
	curl {
		url = 'push.sugrsugr.com:8880/sub/111',
		headers = {
			['If-Modified-Since'] = mtime,
		},
		done = function (r, st)
			done(r)
			if not r then
				info('retry')
				set_timeout(function ()
					get(done)
				end, 1000)
			else
				get(done)
			end
		end,
		on_header = function (k, v)
			if k == 'Last-Modified' then
				mtime = v
				prop.set('comet.mtime', mtime)
			end
		end,
	}
end

get(function (r)
	info(r)
end)

