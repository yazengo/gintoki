
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

R.next = function (done)
	R.source.next(done)
end

R.prev = function (o)
	R.source.prev(done)
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

R.next = function (o, done)
	R.source.next(o, done)
end

R.stop = function ()
	R.source.stop()
end

R.skip = function ()
	R.source.skip()
end

R.source_on_skip = function ()
	if R.on_skip then R.on_skip() end
end

R.change = function (o)
	info('radio.change', o)

	local to = R.name2obj(o.type)
	if to and R.source ~= to then 
		prop.set('radio.default', o.type) 
		if R.source then R.source.stop() end
		R.source = to
		to.on_skip = R.source_on_skip
	end

	if o.autostart ~= false then
		R.skip()
	end
end

radio = R

