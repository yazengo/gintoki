
zpnp = {}

zpnp_on_action = function (a, done)
	if zpnp.on_action then
		a = cjson.decode(a)
		info('<', a)
		zpnp.on_action(a, function (r)
			info('>', r)
			done(cjson.encode(r))
		end)
	end
end

zpnp.notify = function (r)
	info('>', r)
	zpnp_notify(cjson.encode(r))
end

zpnp.start = function ()
	zpnp_start()
	zpnp_setopt{uuid=hostuuid(), name=hostname()}

	local times = 8
	local notify
	notify = function ()
		zpnp_notify()
		times = times - 1
		if times > 0 then set_timeout(notify, 300) end
	end
	notify()
end

