
require('pipe')

local S = {}

local host = '127.0.0.1'
local port = 3389

local function spawn ()
	local p = pexec(string.format('shairport_linux -o tcp %s %d >/tmp/shairport.log', host, port), 'c')

	p[1].exit_cb = function (code)
		pexec('pkill -f avahi-publish-service')
		set_timeout(spawn, 1000)
	end

	S.kill = function ()
		info('kill')
		p[1].kill(9)
	end
end

S.start = function (handler)
	spawn()
	tcpsrv(host, port, function (r)
		handler(r)
	end)
end

S.init = function (done)
	local p = pexec('which shairport_linux', 'c')

	p[1].exit_cb = function (code)
		if code ~= 0 then
			panic('shairport_linux not found')
		end
		info('shairport_linux found')
		done()
	end
end

shairport = S

