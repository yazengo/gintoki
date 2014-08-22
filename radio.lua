
M = {}

emitter_init(M)

M.vol = 100
M.info = function () 
	return {
		battery=90,
		volume=M.vol,
		wifi={ssid="Sugr"},
		firmware_version="1.0.1",
		firmware_need_update=true,
	}
end

M.set_vol = function (vol) 
	M.vol = vol
	M.emit('stat_change')
	return vol
end

muno = M

local A = {}

emitter_init(A)

A.dur = 0
A.pos = 0

A.stat = 'buffering'

A.play = function (song)
	if A.stat ~= 'stopped' then
		clear_timeout(A.timer)
		info('playing stopped')
	end

	A.stat = 'buffering'
	A.dur = song.duration

	info('buffering ' .. song.title)
	A.emit('stat_change')

	A.timer = set_timeout(function () 
		A.stat = 'playing'
		A.ts_start = os.time()

		info('playing ' .. song.title)
		A.emit('stat_change')

		A.timer = set_timeout(function ()
			info('done ' .. song.title)
			A.stat = 'stopped'
			A.emit('done')
		end, song.duration*1000)

	end, 1000)
end

A.next = function ()
	info('next')
	clear_timeout(A.timer)
	A.emit('done')
end

A.pause = function () 
	if A.stat == 'playing' then 
		info('pause')
		A.stat = 'paused'
		A.emit('stat_change')
	end
end

A.resume = function ()
	if A.stat == 'paused' then
		info('resume')
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
		r.position = os.time() - A.ts_start
	elseif A.stat == 'paused' then
		r.duration = A.dur
		r.position = os.time() - A.ts_start
	elseif A.stat == 'buffering' then
		r.position = 0
	end
	return r
end

audio = A

local R = {}

emitter_init(R)

local urls = {
	'http://img.xiami.net/images/album/img48/54548/299932.jpg',
	'http://sfault-avatar.b0.upaiyun.com/143/624/1436242287-1030000000158255_huge128',
	'http://img.xiami.net/images/album/img94/10594/572641372578067.jpg',
}

R.songs = {
	{title='扛把子的光辉照大地', artist='习近平', album='Best of K.B.Z', duration=10, cover_url=urls[1], id='01'},
	{title='Superstar K.B.Z', artist='S.H.E', album='Essential of K.B.Z', duration=10, cover_url=urls[2], id='02'},
	{title='唱支山歌给 K.B.Z 听', artist='邓小平', album='K.B.Z Collection 80s-90s', duration=10, cover_url=urls[3], id='03'},
}
R.songs_i = 0

R.cursong = function () 
	return R.songs[R.songs_i+1]
end

R.info = function ()
	local r = { type='pandora', station='POP' }
	return table.add(r, R.cursong())
end

R.next = function ()
	info('radio next')
	R.songs_i = (R.songs_i + 1) % table.maxn(R.songs)
	R.emit('play', R.cursong())
end

R.prev = function ()
	R.songs_i = (audio.songs_i - 1) % table.maxn(audio.songs)
	R.emit('play', R.cursong())
end

radio = R

