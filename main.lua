
--[[	

radio.start_get_songs()
radio.on('songs', function (songs) 
end)

--]]

muno = {}
muno.vol = 100
muno.info = function () 
	return {
		battery=90,
		volume=muno.vol,
		wifi={ssid="Sugr"},
		firmware_version="1.0.1",
		firmware_need_update=true,
	}
end

local A = {}

A.dur = 0
A.pos = 0

A.stat = 'stopped'

A.play = function (song)
	A.stat = 'buffering'
	A.dur = song.duration

	info('buffering ' + song.title)

	set_timeout(function () 
		A.stat = 'playing'
		info('playing ' + song.title)
		A.emit('stat_change')

		A.ts_start = os.time()
		A.timer = set_timeout(function ()
			info('done ' + song.title)
			A.stat = 'stopped'
			A.emit('done')
		end, song.duration*1000)

	end, 3000)
end

A.next = function ()
	clear_timeout(A.timer)
	A.emit('done')
end

A.pause = function () 
	if A.stat == 'playing' then 
		A.stat = 'paused'
		A.emit('stat_change')
	end
end

A.resume = function ()
	if A.stat == 'paused' then
		A.stat = 'playing'
		A.emit('stat_change')
	end
end

A.play_pause_toggle = function ()
	if A.stat == 'playing' then A.pause() end
	if A.stat == 'paused' then A.resume() end
end

A.info = function ()
	local r = { stat=A.stat }
	if A.stat == 'playing' then
		r.duration = A.dur
	elseif A.stat == 'paused' then
	end
end

audio = A

local R = {}
emitter_init(R)

kbzurl = 'http://sfault-avatar.b0.upaiyun.com/143/624/1436242287-1030000000158255_huge128'

R.songs = {
	{title='扛把子的光辉照大地', artist='习近平', album='Best of K.B.Z', duration=10, cover_url=kbzurl},
	{title='Superstar K.B.Z', artist='S.H.E', album='Essential of K.B.Z', duration=10, cover_url=kbzurl},
	{title='唱支山歌给 K.B.Z 听', artist='邓小平', album='K.B.Z Collection 80s-90s', duration=10, cover_url=kbzurl},,
}
R.songs_i = 0
R.stat = 'playing'

R.info = function ()
	local song = R.songs[R.songs_i]
	local r = { type='pandora', station='POP' }
	table.add(r, song)
	return r
end

R.next = function ()
	R.songs_i = (R.songs_i + 1) % table.maxn(R.songs)
end

R.prev = function ()
	R.songs_i = (audio.songs_i - 1) % table.maxn(audio.songs)
end

R.on('stat_change', function () 
end)

radio = R

local U = {}

U.on('subscribe', function ()
	return { ['muno.info']=muno.info(), ['audio.info']=audio.info() }
end)

U.on('action', function (a)
	a = a or {}

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

upnp = U

