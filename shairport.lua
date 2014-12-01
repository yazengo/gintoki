
require('pipe')

local S = {}

S.start = function (handler)
	tcpsrv('127.0.0.1', 3389, function (r)
		info('start')
		handler(r)
	end)
end

shairport = S

