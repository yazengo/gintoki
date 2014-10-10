
upnp.on('subscribe', function ()
	info('upnp subscribe')
	return { ['muno.info']=muno.info(), ['audio.info']=table.add(radio.info(), audio.info()) }
end)

upnp.on('action', function (a)
	a = a or {}
	
	info('upnp action: ' .. cjson.encode(a))

	if a.op == 'audio.volume' then 
		vol = muno.set_vol(a.value)
		return {result=vol} 
	elseif a.op == 'audio.next' then
		radio.next()
		return {result=0}
	elseif a.op == 'audio.prev' then
		radio.next()
		return {result=0}
	end

	return {result=0}
end)

muno.on('stat_change', function () 
	info('muno stat change: ' .. cjson.encode(muno.info()))
	upnp.notify{['muno.info']=muno.info()}
end)

audio.on('stat_change', function ()
	local r = table.add(audio.info(), radio.info())
	info('audio stat change: ' .. cjson.encode(r))
	upnp.notify{['audio.info']=r}
end)

radio.on('play', function () 
	info(cjson.encode(radio.info()))
	audio.play(radio.cursong())
end)

audio.on('done', function ()
	radio.next()
end)

audio.play(radio.cursong())

