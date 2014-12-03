
require('pipe')

local P = {}

P.songs = function (songs)
	songs = songs or {}

	local r = {}

	r.setsongs = function (songs)
		r.songs = songs
		r.at = 0
		return r
	end

	r.mode = songs.mode
	r.setsongs(songs)

	local modes = { 'repeat_all', 'shuffle' }

	r.setmode = function (m)
		if table.contains(modes, m) then
			r.mode = m
		end
		return r
	end

	local function fetch (o)
		local n = table.maxn(r.songs)

		if r.mode == 'random' then
			r.at = math.random(n)
		else
			if o and o.prev then
				r.at = r.at - 1
			else
				r.at = r.at + 1
			end
		end

		if r.at > n then
			if r.mode == 'repeat_all' then
				r.at = 1
			elseif r.mode == nil then
				return
			end
		elseif r.at < 1 then
			if r.mode == 'repeat_all' then
				r.at = n
			elseif r.mode == nil then
				return
			end
		end

		local song = r.songs[r.at]
		return song
	end

	r.fetch = function (done, o)
		r.fetch_imm = set_immediate(function ()
			local song = fetch(o)
			if song then 
				done(song)
			end
			r.fetch_imm = nil
		end)
	end

	r.cancel_fetch = function ()
		if r.fetch_imm then
			cancel_immediate(r.fetch_imm)
			r.fetch_imm = nil
		end
	end

	r.jump_to = function (i)
		local n = table.maxn(r.songs)

		if i < 1 or i > n then 
			return
		end

		r.at = i
		if r.skip then r.skip() end
	end

	return r
end

P.urls = function (urls)
	urls = urls or {}

	local r = P.songs(songs)
	
	r.seturls = function (urls)
		local songs = {}
		for _, url in pairs(urls) do
			table.insert(songs, {title=basename(url), url=url})
		end
		r.setsongs(songs)
		return r
	end

	r.seturls(urls)

	return r
end

P.station = function (S)
	local task
	local songs = {}
	local songs_i = 1
	local fetch_imm
	local fetch_done
	local fetching

	local task_done
	local task_fail
	local task_new
	local task_is_running
	local task_cancel
	local task_retry_timer

	task_done = function (t, r) 
		if not r or table.maxn(r) == 0 then panic('must have songs') end
		table.append(songs, r)

		if fetching then 
			fetching = false
			local s = songs[songs_i]
			songs_i = songs_i + 1
			fetch_done(s)
		end
	end

	task_fail = function (t, err)
		local wait = 3000
		info('fail', err)
		if t.type == 'fetch' then
			info('retry fetch in', wait, 'ms')
			task_retry_timer = set_timeout(function ()
				task_retry_timer = nil
				task = task_new{type='fetch'}
				S.prefetch_songs(task)
			end, wait)
		end
	end

	task_is_running = function ()
		return (task and not task.finished) or task_retry_timer
	end

	task_cancel = function ()
		if task and not task.finished then
			task.cancel()
		elseif task_retry_timer then
			info('retry cancelled')
			clear_timeout(task_retry_timer)
			task_retry_timer = nil
		end
	end
	
	task_new = function(o)
		o.timeout = o.timeout or 60*1000

		local t = {type=o.type}

		t.done = function (r, ...)
			if t.finished then return end
			t.finished = true
			if t.on_done then t.on_done(r, ...) end
			clear_timeout(t.timer)
			task_done(t, r)
		end

		t.fail = function (err)
			if t.finished then return end
			t.finished = true
			t.err = err
			if t.on_fail then t.on_fail(err) end
			clear_timeout(t.timer)
			task_fail(t, err)
		end

		t.cancel = function ()
			if t.finished then return end
			t.log('cancelled')
			t.finished = true
			if t.on_cancelled then t.on_cancelled() end
			clear_timeout(t.timer)
		end

		t.log = function (...)
			if t.finished then return end
			if t then info(S.name, 'task:', ...) end
		end

		t.timer = set_timeout(t.cancel, o.timeout)
		return t
	end

	S.restart_and_fetch_songs = function ()
		task_cancel()
		songs = {}
		songs_i = 1
		task = task_new{type='restart'}
		return task
	end

	S.fetch = function (done)
		if fetching then
			panic('call fetch() before last ends')
		end
		fetching = true
		fetch_done = done

		local s = songs[songs_i]

		if s then
			-- just return
			songs_i = songs_i + 1
			fetch_imm = set_immediate(function () 
				fetch_imm = nil
				fetching = false
				fetch_done(s)
			end)
			if songs_i + 2 > table.maxn(songs) and not task then
				task = task_new{type='fetch'}
				S.prefetch_songs(task)
			end
		elseif task_is_running() then
			-- wait for end
		else
			-- new task
			task = task_new{type='fetch'}
			S.prefetch_songs(task)
		end
	end

	S.cancel_fetch = function ()
		if fetching then fetching = nil end
		if fetch_imm then cancel_immediate(fetch_imm) end
	end

	return S
end

P.switcher = function ()
	local sw = {}
	local fetch_cb
	local stat = 'closed' -- 'closed / stopped / fetching / prefetch'

	local function fetch_done (...)
		stat = 'stopped'
		fetch_cb(...)
	end

	sw.setsrc = function (src)
		local old_stat = stat
		if stat == 'closed' then
			if src then
				sw.src = src
				stat = 'stopped'
			end
		elseif stat == 'stopped' then
			if src then
				sw.src = src
			else
				sw.src = nil
				stat = 'closed'
			end
		elseif stat == 'fetching' then
			if src then
				sw.src.cancel_fetch()
				sw.src = src
				sw.src.fetch(fetch_done)
			else
				sw.src.cancel_fetch()
				sw.src = nil
				stat = 'closed'
			end
		elseif stat == 'prefetch' then
			if src then
				sw.src = src
				sw.src.fetch(fetch_done)
				stat = 'fetching'
			end
		end
		info(old_stat, '->', stat)
	end

	sw.fetch = function (done)
		if stat == 'stopped' then
			fetch_cb = done
			sw.src.fetch(fetch_done)
			stat = 'fetching'
		elseif stat == 'closed' then
			fetch_cb = done
			stat = 'prefetch'
		else
			panic('stat', stat, 'invalid')
		end
	end

	sw.cancel_fetch = function ()
		if stat == 'fetching' then
			sw.src.cancel_fetch()
			stat = 'stopped'
		elseif stat == 'prefetch' then
			stat = 'closed'
		end
	end
	
	return sw
end

P.player = function (src)
	local stat = 'fetching' -- 'fetching / probing / playing / closing / closed'

	local p0 = pipe.new()
	local p1 = pipe.new()
	local c0
	local c1
	local dec
	local fetch
	local fetch_o
	local fetch_done
	local play_done
	local play_failed
	local statchanged_cb

	c1 = pipe.copy(p0, p1, 'rw').done(function (reason)
		stat = 'closed'
		if p1.done_cb then p1.done_cb(reason) end
	end)

	p1.stat = function ()
		if stat == 'playing' then
			return c1.stat
		else
			return stat
		end
	end

	p1.close = function ()
		if stat ~= 'closing' and stat ~= 'closed' then
			c1.close()
		end
	end

	p1.done = function (cb)
		p1.done_cb = cb
		return p1
	end

	p1.pos = function ()
		return c1.rx() / (44100.0*4)
	end

	local function statchanged (r)
		if statchanged_cb then statchanged_cb(r) end
	end

	p1.statchanged = function (cb)
		statchanged_cb = cb
		return p1
	end
	
	p1.pause  = c1.pause
	p1.resume = c1.resume
	p1.pause_resume = c1.pause_resume

	play_done = function (reason)
		if reason == 'w' then 
			pipe.close_write(p0)
			return
		end
		fetch()
	end

	play_failed = function ()
		set_timeout(fetch, 500)
	end

	fetch_done = function (song)
		p1.song = song

		if song == nil then
			stat = 'closing'
			pipe.close_write(p0)
			return
		end
		
		stat = 'probing'

		dec = audio.decoder(song.url).probed(function ()
			stat = 'playing'
			p1.dur = dec.dur
			c0 = pipe.copy(dec, p0, 'r').done(play_done)
			c1.statchanged(statchanged)
		end).failed(play_failed)
	end

	fetch = function ()
		stat = 'fetching'
		src.fetch(fetch_done, fetch_o)
		fetch_o = nil
	end

	local function skip ()
		if stat == 'playing' then c0.close() end
		if stat == 'probing' then dec.stop() end
	end

	p1.next = function ()
		skip()
	end
	
	p1.prev = function ()
		skip()
		fetch_o = {prev=true}
	end

	src.skip = skip
	fetch()

	return p1
end

playlist = P

