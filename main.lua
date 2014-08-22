
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

local L = {}

L.list = {
	{stat='buffering', position=0, i=0, t=3000},
	{stat='playing', position=0, i=0, t=3000},
	{stat='paused', position=3, i=0, t=3000},
	{stat='playing', position=3, i=0, t=3000},

	{stat='buffering', position=0, i=1, t=3000},
	{stat='playing', position=0, i=1, t=3000},
	{stat='paused', position=3, i=1, t=3000},
	{stat='playing', position=3, i=1, t=3000},

	{stat='buffering', position=0, i=2, t=3000},
	{stat='playing', position=0, i=2, t=3000},
	{stat='paused', position=3, i=2, t=3000},
	{stat='playing', position=3, i=2, t=7000},

	{stat='fetching'},
}
L.i = 0
L.next = function () 
	if L.i >= table.maxn(L.list) then
		return
	end

	local l = L.list[L.i+1]
	local song = {}

	if l.i then song = radio.songs[l.i+1] end
	local p = {['audio.info']=table.add(song, l)}

	L.i = L.i + 1
	return p, l.t
end

local timer

local test
test = function () 
	r, tm = L.next()
	upnp.notify(r)
	if tm then set_timeout(test, tm) end
end

upnp.on('subscribe', function ()
	info('upnp subscribe')
	L.i = 0
	clear_timeout(timer)
	timer = set_timeout(test, 3000)
	return { ['muno.info']=muno.info(), ['audio.info']=table.add(radio.info(), audio.info()) }
end)

