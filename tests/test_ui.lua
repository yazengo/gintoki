
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

