
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
	zpnp_setopt{uuid=hostuuid(), name=hostuuid()}
	zpnp.online()
end

pnp = {}
pnp.init = function ()
	pnp.notify = function () end
	pnp.online = function () end
	pnp.stop = function () end
end

pnp.init()

pnp.start = function ()
	zpnp.start()
	zpnp.on_action = function (...)
		if pnp.on_action then
			pnp.on_action(...)
		end
	end
	pnp.notify = function (r) zpnp.notify(r) end
	pnp.online = zpnp.online
	pnp.stop = function ()
		zpnp.stop()
		pnp.init()
	end
end

pnp.notify_event = function (r) pnp.notify(table.add(r, {type='event'})) end
pnp.notify_sync  = function (r) pnp.notify(table.add(r, {type='sync'})) end

