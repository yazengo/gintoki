
require('pipe')

local S = {}

S.start = function (handler)
	tcpsrv('127.0.0.1', 3389, function (r)
		info('start')
		local sink = pdirect()
		pipe.copy(r, sink, function (reason)
			if reason == 'w' then pexec('pkill shairport') end
		end)
		handler(sink)
	end)
end

shairport = S

