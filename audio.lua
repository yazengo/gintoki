
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

