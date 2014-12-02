
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
	end

	local function fetch(o)
		local url = r.urls[r.at]
		local n = table.maxn(r.urls)

		if r.mode ~= 'random' then
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
			end
		elseif r.at < 1 then
			if r.mode == 'repeat_all' then
				r.at = n
			elseif r.mode == nil then
				r.at = 1
			end
		end

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
	local p = pdirect()

	p.stat = 'stopped' 
	p.paused = false

	-- fetching
	-- playing
	-- buffering
	-- changing_src
	-- stopped
	-- closed

	p.pos = function ()
		if p.stat == 'playing' or p.stat == 'buffering' then
			return p.copy.rx() / (44100*4)
		end
	end

	p.on_change = function ()
		local r = {stat=p.stat}

		if p.paused and r.stat == 'playing' then r.stat = 'paused' end
		if p.song then r.song = table.copy(p.song) end

		set_immediate(function ()
			if r.stat == 'closed' and p.done_cb then p.done_cb() end
			if p.changed_cb then p.changed_cb(r) end
		end)
	end

	p.done = function (cb)
		p.done_cb = cb
	end

	p.changed = function (cb)
		p.changed_cb = cb
	end

	p.fetch = function ()
		p.src.fetch(p.src_fetch, p.fetch_o)
		p.fetch_o = nil
	end

	p.pause = function ()
		if p.paused then return end
		p.paused = true

		if p.stat == 'playing' or p.stat == 'buffering' then
			p.copy.pause()
			p.on_change()
		elseif p.stat == 'fetching' then
			p.src.cancel_fetch()
			p.on_change()
		end
	end

	p.resume = function ()
		if not p.paused then return end
		p.paused = false

		if p.stat == 'playing' or p.stat == 'buffering' then
			p.copy.resume()
			p.on_change()
		elseif p.stat == 'fetching' then
			p.fetch()
			p.on_change()
		end
	end

	p.next = function ()
		if p.stat == 'playing' or p.stat == 'buffering' then
			p.copy.close()
		end
	end

	p.prev = function ()
		p.fetch_o = {prev=true}

		if p.stat == 'playing' or p.stat == 'buffering' then
			p.copy.close()
		elseif p.stat == 'fetching' then
			if not p.paused then
				p.src.cancel_fetch()
				p.fetch()
			end
		end
	end

	p.pause_resume = function ()
		if p.paused then
			p.resume()
		else
			p.pause()
		end
	end

	p.src_fetch = function (song)
		if not isstring(song.url) then
			panic('url must be set')
		end

		p.song = song
		p.url = song.url
		p.dec = audio.decoder(p.url)

		p.stat = 'buffering'
		p.on_change()

		p.dec.probed(function (d)
			p.dur = d.dur
		end)

		local c = pipe.copy(p.dec, p, 'r')

		c.buffering(1500, function (buffering)
			if buffering and p.stat == 'playing' then
				p.stat = 'buffering'
				if not p.paused then p.on_change() end
			elseif not buffering and p.stat == 'buffering' then
				p.stat = 'playing'
				if not p.paused then p.on_change() end
			end
		end)
		
		c.done(function (reason)
			info('done', 'stat=', p.stat, 'reason=', reason)
			p.copy = nil

			if reason == 'w' then
				-- EOF
				pclose_write(p)
				p.stat = 'closed'
				p.on_change()
				return
			end

			if p.stat == 'playing' or p.stat == 'buffering' or p.stat == 'changing_src' then
				p.stat = 'fetching'
				p.on_change()
				if not p.paused then p.fetch() end
			elseif p.stat == 'closing' then
				p.stat = 'closed'
				p.on_change()
			end
		end)

		p.copy = c
	end

	p.src_skip = function ()
		if p.stat == 'playing' or p.stat == 'buffering' then 
			p.copy.close()
		end
	end

	p.src_close = function ()
		p.src.close = nil
		p.src.skip = nil

		if p.stat == 'playing' or p.stat == 'buffering' then
			p.copy.close()
			p.stat = 'closing'
		elseif p.stat == 'fetching' then
			info('close write')
			pclose_write(p)
			p.src.cancel_fetch()
			p.stat = 'closed'
			p.on_change()
		end

		p.src = nil
	end

	p.setsrc = function (src)
		src.skip = p.src_skip
		src.close = p.src_close

		if p.stat == 'playing' or p.stat == 'buffering' then
			p.stat = 'changing_src'
			p.copy.close()
			p.src = src
		elseif p.stat == 'fetching' and not p.paused then
			p.src.cancel_fetch()
			p.src = src
			p.fetch()
		elseif p.stat == 'stopped' then
			p.stat = 'fetching'
			p.on_change()
			p.src = src
			p.fetch()
		elseif p.stat == 'closed' then
			panic('aleady closed')
		end

		return p
	end

	return p
end

playlist = P

