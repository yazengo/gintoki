
require('pipe')

tcpsrv('127.0.0.1', 8989, function (r)
	pipe.readall(r, function (s)
		info(s)
	end)
end)

