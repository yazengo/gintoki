
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
	upnp.loadconfig()
	upnp_start()
end

upnp.stop = function ()
	upnp_end()
end

upnp.loadconfig = function ()
	local tpl = io.open('upnpweb/munodevicedesc.tpl.xml', 'r')
	if not tpl then error('upnp tpl open failed') end
	local xml = io.open('upnpweb/munodevicedesc.xml', 'w+')
	if not xml then error('upnp xml open failed') end

	local name = hostname()
	local s = tpl:read('*a')
	s = string.gsub(s, '{NAME}', name)
	xml:write(s)

	tpl:close()
	xml:close()
end

