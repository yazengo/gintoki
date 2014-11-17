
require('prop')

comet = function (o)
	local C = {}

	C.hash = string.sub(sha1_encode(o.url), -7)
	C.mt = 'comet.mtime.' .. C.hash

	o.retry = o.retry or 2000

	C.poll = function ()
		curl {
			url = o.url,
			headers = {
				['If-Modified-Since'] = prop.get(C.mt),
			},

			done = function (r, st)
				o.handler(r)
				if not r then
					set_timeout(function ()
						C.poll()
					end, o.retry)
				else
					C.poll()
				end
			end,

			on_header = function (k, v)
				if k == 'Last-Modified' then
					prop.set(C.mt, v)
				end
			end,
		}
	end

	C.poll()
end

