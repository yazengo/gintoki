
require('prop')
require('localmusic')
require('pandora')
require('douban')
require('bbcradio')

local R = {}

R.info = function ()
	local r = {}
	table.add(r, R.source.info())
	table.add(r, R.song or {})
	return r
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

R.fetching = false

R.next_done = function (song)
	R.fetching = false
	R.next_cb(song)
end

R.next = function (o, done)
	R.fetching = true
	R.next_cb = done
	R.source.next(o, R.next_done)
end

R.stop = function ()
	R.source.stop()
end

R.setopt = function (o, done)
	if o.op == 'local.toggle_repeat_mode' and R.source == slumbermusic then
		o.op = 'slumber.toggle_repeat_mode'
	end

	if o.op == 'audio.play' then
		if R.source == slumbermusic then o.op = 'slumber.play' end
		if R.source == localmusic then o.op = 'local.play' end
	end

	local ok

	if string.hasprefix(o.op, 'local.') then
		ok = localmusic.setopt(o, done)
	elseif string.hasprefix(o.op, 'pandora.') then
		ok = pandora.setopt(o, done)
	elseif string.hasprefix(o.op, 'bbcradio.') then
		ok = bbcradio.setopt(o, done)
	elseif string.hasprefix(o.op, 'douban.') then
		ok = douban.setopt(o, done)
	elseif string.hasprefix(o.op, 'slumber.') then
		ok = slumbermusic.setopt(o, done)
	elseif o.op == 'audio.next' then
		R.do_skip()
		done{result=0}
		ok = true
	elseif o.op == 'audio.prev' then
		ok = R.source.setopt(o, done)
	elseif o.op == 'radio.change_type' then
		R.change(o)
		done{result=0}
		ok = true
	end

	return ok
end

R.name2obj = function (s)
	if s == 'pandora' then
		return pandora
	elseif s == 'local' then
		return localmusic
	elseif s == 'slumber' then
		return slumbermusic
	elseif s == 'bbcradio' then
		return bbcradio
	elseif s == 'douban' then
		return douban
	end
end

R.do_skip = function ()
	if R.fetching then
		R.stop()
		R.source.next(o, R.next_done)
	else
		if R.on_skip then R.on_skip() end
	end
end

R.set_source = function (to)
	R.source = to
	R.source.on_skip = R.do_skip
end

R.change = function (o)
	info('radio.change', o)
	local to = R.name2obj(o.type)

	if to and R.source ~= to then 
		prop.set('radio.default', o.type) 
		if R.source then R.source.stop() end
		R.set_source(to)
		if o.autostart ~= false then
			R.do_skip()
		end
	end
end

R.set_source(localmusic)

radio = R

