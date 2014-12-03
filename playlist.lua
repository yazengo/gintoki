
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
				r.close()
				return
			end
		elseif r.at < 1 then
			if r.mode == 'repeat_all' then
				r.at = n
			elseif r.mode == nil then
				r.close()
				return
			end
		end

		local song = r.songs[r.at]

		if r.mode == 'random' then
			at = math.random(n)
		else
		end

		info('pos', r.at, '->', at)
		r.at = at

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

P.player = function ()
	local fetching
	local fetcher
	local fetch_opt

	local closed

	local p0 = pdirect()
	local p1 = pdirect()

	local c0
	local c1 = pipe.copy(p0, p1, 'rw').done(function (reason)
		info('closed')
		closed = true
	end)

	local statchanged_cb
	local function statchanged (r)
		if statchanged_cb then statchanged_cb(r) end
	end

	p1.close = function ()
		c1.close()
	end

	p1.stat = function ()
		if closed then return 'closed' end
		if fetching then return 'fetching' end
		return c1.stat
	end

	p1.pos = function ()
		return c1.rx() / (44100.0*4)
	end

	p1.statchanged = function (cb)
		statchanged_cb = cb
		return p1
	end
	
	p1.pause  = c1.pause
	p1.resume = c1.resume
	p1.pause_resume = c1.pause_resume

	local function fetch ()
		if fetcher == nil then return end

		fetching = true
		fetcher.fetch(function (song)
			p1.song = song
			fetching = nil

			local dec = audio.decoder(song.url).probed(function (d)
				p1.dur = d.dur
			end)

			c0 = pipe.copy(dec, p0, 'r').done(function (reason)
				c0 = nil

				if reason == 'w' then 
					pclose_write(p0)
					return
				end

				fetch()
			end)

			c1.statchanged(statchanged)
		end, fetch_opt)
		fetch_opt = nil
	end

	local function skip ()
		if c0 then c0.close() end
	end

	p1.setsrc = function (src)
		if closed then panic('already closed') end

		if src then

			if fetcher then

				if fetching then
					fetcher.cancel_fetch()
					fetcher = src
					fetch()
				else
					fetcher = src
					c0.close()
				end

			else

				fetcher = src
				fetch()

			end

		else

			if fetcher then

				if fetching then
					fetcher.cancel_fetch()
					fetcher = nil
				else
					fetcher = nil
					c0.close()
				end

			else

				-- do nothing

			end
		end

		if fetcher then
			fetcher.skip = skip
		end
	end

	p1.next = function ()
		if c0 then c0.close() end
	end
	
	p1.prev = function ()
		if c0 then c0.close() end
		fetch_opt = {prev=true}
	end

	p1.src = function ()
		return fetcher
	end

	return p1
end

playlist = P

