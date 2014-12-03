
require('pipe')

audio = {}

audio.out = aout

audio.noise = function ()
	return asrc_new('noise')
end

audio.decoder = function (url)
	local mode = 're'

	if not url then
		url = '-'
		mode = 'wer'
	end

	local p = pexec(string.format('avconv -i "%s" -f s16le -ar 44100 -ac 2 -', url), mode)
	if p[3] then
		pipe_setopt(p[3], 'read_mode', 'block')
	elseif p[1] then
		pipe_setopt(p[1], 'read_mode', 'block')
	end

	local d = {p[1], p[3]}

	d.probed = function (cb)
		d.probed_cb = cb
		return d
	end

	pipe.grep(p[2], 'Duration:', function (s)
		local hh, mm, ss = string.match(s, 'Duration: (%d+):(%d+):(%d+)')
		if hh and mm and ss then
			d.dur = hh*3600 + mm*60 + ss
		else
			d.dur = 0
		end
		if d.probed_cb then d.probed_cb(d) end
	end)

	return d
end

audio.mixer = function ()
	local m = amixer_new()

	m.add = function ()
		return amixer_setopt(m, 'track.add')
	end

	return m
end

audio.pipe = function (...)
	local r = {}
	local a = {...}
	local n = table.maxn(a)

	if table.maxn(a) < 2 then
		panic('at least 2 node')
	end

	for i = 1, n do
		local min
		if i == 1 or i == n then min = 1 else min = 2 end

		local p = a[i]
		if not p[1] then
			p = {p}
		end
		a[i] = p

		if table.maxn(p) < min then
			panic('at least', min, 'pipe at #', i)
		end
	end

	local first = a[1]
	local last = a[n]

	if first[2] then
		table.insert(r, first[1])
	else
		first[2] = first[1]
	end
	if last[2] then
		table.insert(r, last[2])
	end

	local c
	for i = 1, n-1 do 
		c = pipe.copy(a[i][2], a[i+1][1], 'brw')
	end

	r.done = c.done

	return r
end

local roundvol = function (v)
	if v > 1.0 then 
		return 1.0 
	elseif v < 0.0 then 
		return 0.0
	else
		return v
	end
end

audio.effect = function ()
	local e, w, r = aeffect_new()
	local o = {w, r}

	o.fadein = function (time)
		aeffect_setopt(e, 'fadein', time)
		return o
	end

	o.fadeout = function (time)
		aeffect_setopt(e, 'fadeout', time)
		return o
	end

	o.getvol = function ()
		return aeffect_setopt(e, 'getvol')
	end

	o.setvol = function (vol)
		aeffect_setopt(e, 'setvol', roundvol(vol))
		return o
	end

	o.cancel_fade = function ()
		aeffect_setopt(e, 'cancel_fade')
		return o
	end

	return o
end

audio.switcher = function ()
	local sw = pdirect()
	local src, p_cur, c_cur, p_breakin

	local function copy(p)
		local mode = 'r'
		if p == src then
			mode = ''
		end

		p_cur = p
		c_cur = pipe.copy(p, sw, mode).done(function (reason)
			if p_cur == p then
				c_cur = nil
			end

			if reason == 'w' then
				pclose_write(sw)
				sw = nil
				return
			end

			if src and c_cur == nil then
				copy(src)
			end
		end)
	end

	local function play(p)
		if sw == nil then 
			panic('already closed')
		end

		if c_cur then
			c_cur.close()
			c_cur = nil
		end

		copy(p)
	end

	sw.setsrc = function (p)
		if p == nil then
			if src then 
				src = nil
			end
		elseif src then
			if c_cur and p_cur == src then
				c_cur.close()
				c_cur = nil
			end
			src = p
		else
			src = p
			if c_cur == nil then
				play(p)
			end
		end

		return sw
	end

	sw.stop_breakin = function ()
		if c_cur and p_breakin == p_cur then
			c_cur.close()
			c_cur = nil
		end
	end

	sw.breakin = function (p)
		p_breakin = p
		play(p)
		return sw
	end

	return sw
end

