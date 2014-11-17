
local R = {}

R.canceller = function ()
	local C = {}

	C.cancel = function ()
		C.cancelled = true
	end

	C.wrap = function (func)
		return function ()
			if not C.cancelled then func() end
		end
	end

	return C
end

R.new_station = function (S)
	local call2 = function (a, b)
		return function (...)
			a(...)
			b(...)
		end
	end

	S.songs = {}
	S.songs_i = 1

	S.skip = function ()
		if s.on_skip then s.on_skip() end
	end

	S.song_next = function ()
		if S.songs_i <= table.maxn(S.songs) then
			S.songs_i = S.songs_i+1
			return S.songs[S.songs_i-1]
		end
	end

	S.restart_done = function (songs)
		table.append(S.songs, songs)
		if S.next_cb then
			S.next_cb(S.song_next())
			S.next_cb = nil
		end
	end

	S.restart_fail = function (...)
	end

	S.restart = function ()
		if S.fetching then
			S.fetching.cancel()
		end
		local task = R.canceller()
		task.done = call2(task.on_done, C.restart_done)
		task.fail = C.restart_fail
		S.fetching = task
		return task
	end

	S.fetch_done = function (songs)
		table.append(S.songs, songs)
		S.next_cb(S.song_next())
		S.fetching = nil
		S.next_cb = nil
	end

	S.fetch_fail = function (err)
		S.fetching = nil
	end

	S.cancel_next = function ()
		S.next_cb = nil
	end

	S.next = function (o, done)
		local song = S.song_next()
		if song then 
			set_timeout(function () done(song) end, 0)
			return
		end
		S.next_o = o
		S.next_cb = done
		if not S.fetching then 
			local task = S.fetch()
			task.done = call2(S.fetch_done, task.on_done)
			task.fail = S.fetch_fail
			S.fetching = task
		end
	end

	return S
end

-- urls = {'http://...', ...}
-- option = {loop=true/false, done=[function]}
R.new_songlist = function (urls, o)
	local S = {}

	S.list = urls
	S.i = 1

	S.stop = function ()
	end

	S.next = function (o, done)
		if S.i > table.maxn(S.list) then
			if not o.loop then
				if S.on_stop then S.on_stop() end
				return
			else
				S.i = 1
			end
		end
		local s = S.list[S.i]
		done({url=s})
		S.i = S.i + 1
	end

	return S
end

radio = R

