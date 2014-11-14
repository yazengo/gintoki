
require('radio')
require('playque')

local C = {}

C.radio_que = playque(0, radio)

C.breaking_enter = function ()
	C.radio_que.pause()
end

C.breaking_leave = function ()
	C.radio_que.resume()
end

C.breaking_radio = function (name, radio, done)
	if C.breaking_que then
		C.breaking_que.stop()
	end

	local p = playque(2, radio)
	p.closed(function ()
		done()
		C.breaking_n = C.breaking_n - 1
		if C.breaking_n == 0 then C.breaking_leave() end
	end)
	p.name = name
	p.start()

	C.breaking_n = C.breaking_n + 1
	C.breaking_que = p
	if C.breaking_n == 1 then C.breaking_enter() end

	return p
end

C.start = function ()
	C.radio_que.start()
end

ctrl = C

