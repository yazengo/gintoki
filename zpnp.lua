
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

zpnp.online = function ()
	local times = 8
	local interval = 300
	local notify
	notify = function ()
		zpnp_notify()
		times = times - 1
		if times > 0 then set_timeout(notify, interval) end
	end
	notify()
end

zpnp.start = function ()
	zpnp_start()
	zpnp.stop = zpnp_stop
	zpnp_setopt{uuid=hostuuid(), name=hostname()}
	zpnp.online()
end

