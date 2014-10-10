
upnp = {}

upnp_on_subscribe = function (r, done)
	if upnp.on_subscribe then
		upnp.on_subscribe(cjson.decode(r), function (r)
			info('>', r)
			done(base64_encode(cjson.encode(r)))
		end)
	end
end

upnp_on_action = function (a, done)
	if upnp.on_action then
		a = cjson.decode(a)
		info('<', a)
		upnp.on_action(a, function (r)
			info('>', r)
			done(cjson.encode(r))
		end)
	end
end

upnp.notify = function (r)
	info('>', r)
	upnp_notify(base64_encode(cjson.encode(r)))
end

upnp.start = function ()
	upnp_start()
end

upnp.stop = function ()
	upnp_end()
end

