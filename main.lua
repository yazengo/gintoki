
--[[

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

--]]

local list = {
	{stat='buffering', position=0},
	{stat='playing', position=0},
	{stat='paused', position=3},
	{stat='playing', position=3},
}
local list_i = 0
local song_i = 0

local test
test = function () 
	local song = radio.songs[song_i+1]
	local t = list[list_i+1]
	local p = {['audio.info']=table.add(song, t)}

	info(cjson.encode(p))
	upnp.notify(p)
	list_i = list_i + 1

	if list_i >= table.maxn(list) then
		song_i = song_i + 1
		list_i = 0
		if song_i >= table.maxn(radio.songs) then
			return
		end
	end
	set_timeout(test, 3000)
end

upnp.on('subscribe', function ()
	info('upnp subscribe')
	set_timeout(test, 3000)
	return { ['muno.info']=muno.info(), ['audio.info']=table.add(radio.info(), audio.info()) }
end)

