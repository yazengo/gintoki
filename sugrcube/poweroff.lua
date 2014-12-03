
local P = {}

local t_interval
local t_timeout
local tm_timeout
local tm_start

local notify_interval = 1000 * 60

P.start = function (timeout, notify)
	P.cancel()

	tm_timeout = timeout
	tm_start = now()

	t_interval = set_interval(function ()
		notify(P.info())
	end, notify_interval)

	t_timeout = set_timeout(function ()
		tm_start = nil
		clear_interval(t_interval)
		arch.poweroff()
	end, tm_timeout)
end

P.cancel = function ()
	if t_interval then clear_interval(t_interval) end
	if t_timeout then clear_timeout(t_timeout) end
	tm_start = nil
end

P.stat = function ()
	if tm_start then
		return {
			timeout = tm_timeout,
			countdown = math.ceil(tm_start - now() + tm_timeout),
		}
	end
end

poweroff = P

