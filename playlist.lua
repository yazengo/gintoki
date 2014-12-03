
local P = {}

P.urls = function (urls)
	local r = {}

	r.seturls = function (urls)
		r.urls = urls
		r.at = 1
		return r
	end
	
	r.mode = urls.mode
	r.seturls(urls)

	r.setmode = function (m)
		r.mode = m
		return r
	end

	local function fetch(o)
		local url = r.urls[r.at]
		local n = table.maxn(r.urls)
		local at

		if r.mode == 'random' then
			at = math.random(n)
		else
			if o and o.prev then
				at = r.at - 1
			else
				at = r.at + 1
			end
		end

		if at > n then
			if r.mode == 'repeat_all' then
				at = 1
			elseif r.mode == nil then
				r.close()
			end
		elseif at < 1 then
			if r.mode == 'repeat_all' then
				at = n
			elseif r.mode == nil then
				at = 1
			end
		end

		info('pos', r.at, '->', at)
		r.at = at
		return url
	end

	r.fetch = function (done, o)
		r.fetch_imm = set_immediate(function ()
			local url = fetch(o)
			if url then 
				done{url=url, title=basename(url)}
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

	r.done = function (cb)
		r.done_cb = cb
	end

	return r
end

P.station = function (S)
	local task
	local songs = {}
	local songs_i = 1
	local fetch_imm
	local fetch_done
	local fetching = false

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
			if songs_i + 1 > table.maxn(songs) and not task then
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
	local function statchanged (st)
		if statchanged_cb then statchanged_cb(st) end
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

	local function fetch ()
		fetching = true
		fetcher.fetch(function (song)
			p1.song = song
			fetching = nil
			local dec = audio.decoder(song.url).probed(function (d)
				p1.dur = d.dur
			end)
			c0 = pipe.copy(dec, p0, 'r').done(function ()
				c0 = nil
				fetch()
			end)
			c1.statchanged(statchanged)
		end, fetch_opt)
		fetch_opt = nil
	end

	p1.setsrc = function (src)
		if closed then
			panic('already closed')
		end

		if fetching then
			fetcher.cancel_fetch()
		end
		if c0 then
			c0.close()
		end
		fetcher = src

		if not fetcher then
			return
		end
		
		fetch()
	end

	p1.next = function ()
		if c0 then c0.close() end
	end
	
	p1.prev = function ()
		if c0 then c0.close() end
		fetch_opt = {prev=true}
	end

	return p1
end

playlist = P

