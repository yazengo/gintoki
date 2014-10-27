
poweroff = function (opt)
	local r = {
		timeout = opt.timeout,
		start = now(),

		cancel = function (p)
			clear_timeout(p.timeout_h)
			clear_interval(p.interval_h)
		end,

		info = function (p)
			return {
				timeout = p.timeout,
				countdown = math.ceil(p.start - now() + p.timeout),
			}
		end,
	}

	r.timeout_h = set_timeout(function ()
		r:cancel()
		opt.done()
	end, r.timeout*1000)

	opt.notify(r:info())

	r.interval_h = set_interval(function ()
		opt.notify(r:info())
	end, 1000*60)

	return r
end

