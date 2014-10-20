
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
end

