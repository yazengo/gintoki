
require('pipe')

audio = {}

audio.out = aout

audio.decoder = function (url)
	local p = pexec(string.format('avconv -i %s -f s16le -ar 44100 -ac 2 -', url), 're')
	local d = {p[1]}

	d.probed = function (cb)
		d.probed_cb = cb
		return d
	end

	pipe.grep(p[2], 'Duration:', function (s)
		local hh, mm, ss = string.match(s, 'Duration: (%d+):(%d+):(%d+)')
		if hh and mm and ss then
			d.dur = hh*3600 + mm*60 + ss
			if d.probed_cb then d.probed_cb() end
		end
	end)

	return d
end

audio.mixer = function ()
	local m, out = amixer_new()

	local roundvol = function (v)
		if v > 1.0 then 
			return 1.0 
		elseif v < 0.0 then 
			return 0.0
		else
			return v
		end
	end

	out.setvol = function (v)
		v = v or 0.0
		amixer_setopt(m, 'setvol', roundvol(v))
	end

	out.getvol = function ()
		return amixer_setopt(m, 'getvol')
	end

	out.add = function (p)
		if p[1] then p = p[1] end

		local tr = amixer_setopt(m, 'track.add', p)

		tr.done = function (cb)
			tr.done_cb = cb
			return tr
		end

		tr.close = function ()
			amixer_track_setopt(tr, 'close')
			return tr
		end

		tr.pause = function ()
			amixer_track_setopt(tr, 'pause')
			return tr
		end

		tr.resume = function ()
			amixer_track_setopt(tr, 'resume')
			return tr
		end

		tr.setvol = function (v)
			v = v or 0.0
			amixer_track_setopt(tr, 'setvol', roundvol(v))
			return tr
		end

		tr.getvol = function ()
			return amixer_track_setopt(tr, 'getvol')
		end

		return tr
	end

	return out
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
		c = pipe.copy(a[i][2], a[i+1][1], 'b')
	end

	r.done = c.done

	return r
end

