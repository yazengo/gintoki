
require('pcopy')

audio = {}

audio.out = aout

audio.decoder = function (url)
	return pexec(string.format('avconv -i %s -f s16le -ar 44100 -ac 2 -', url), 'r')
end

audio.mixer = function ()
	local m, out = amixer_new()

	out.add = function (p)
		if p[1] then p = p[1] end

		local tr = amixer_track_add(m, p)

		tr.done = function (cb)
			tr.done_cb = cb
			return tr
		end

		tr.close = function ()
			amixer_track_close(tr)
			return tr
		end

		tr.pause = function ()
			amixer_track_pause(tr)
			return tr
		end

		tr.resume = function ()
			amixer_track_resume(tr)
			return tr
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
		c = pcopy(a[i][2], a[i+1][1])
	end

	r.done = function (cb)
		c.done(cb)
	end

	return r
end

