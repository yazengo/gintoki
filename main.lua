
--[[
upnp.on('subscribe', function (info, done)
	done({hello='world'})
end)
--]]

upnp.on('subscribe', function ()
	return {hello=1, world=2}
end)

upnp.on('action', function (a)
	a = a or {}
	if a.op == 'audio.volume' then return {result=a.value} end
	return {act=1, aaa=2}
end)

