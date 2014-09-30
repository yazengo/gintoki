
require('localmusic')
require('pandora')
require('douban')
require('bbcradio')
require('airplay')
require('radio')
require('audio')
require('muno')
require('upnp')
require('zpnp')
require('hostname')

local M = {}

M.log = function (...)
	info('main:', ...)
end

ar_info = function ()
	local ai = audio.info()
	local ri = radio.info()
	if ri.fetching then
		ai.stat = 'fetching'
	end
	ri.fetching = nil
	local r = table.add({}, ai, ri)
	if r.url then r.url = nil end
	return r
end

all_info = function (done)
	muno.getinfo(function (r)
		done {
			['audio.info']=ar_info(),
			['muno.info']=r,
		}
	end)
end

handle = function (a, done)
	done = done or function () end
	local fail = function () done{result=1, msg='params invalid'} end
	
	if not a or not a.op or not isstr(a.op) then
		fail()
		return
	end

	if a.op == 'audio.play' then
		a.op = 'local.play'
	end

	if airplay.setopt(a, done) then
		return
	end

	if a.op == 'audio.volume' then 
		local vol = audio.setvol(a.value)
		muno.notify_vol_change(vol)
		done{result=vol}
	elseif a.op == 'muno.info' then
		done(muno.info())
	elseif a.op == 'audio.prev' then
		radio.prev()
		done{result=0}
	elseif a.op == 'audio.next' then
		radio.next()
		done{result=0}
	elseif a.op == 'audio.play_pause_toggle' then
		audio.pause_resume_toggle()
		done{result=0}
	elseif a.op == 'audio.pause' then
		audio.pause()
		done{result=0}
	elseif a.op == 'audio.resume' then
		audio.resume()
		done{result=0}
	elseif string.hasprefix(a.op, 'local.') then
		if not localmusic.setopt(a, done) then fail() end
	elseif string.hasprefix(a.op, 'pandora.') then
		if not pandora.setopt(a, done) then fail() end
	elseif string.hasprefix(a.op, 'bbcradio.') then
		if not bbcradio.setopt(a, done) then fail() end
	elseif string.hasprefix(a.op, 'douban.') then
		if not douban.setopt(a, done) then fail() end
	elseif a.op == 'radio.change_type' then
		radio.change(a)
		done{result=0}
	elseif a.op == 'muno.check_update' then
		muno.check_update(done)
	elseif a.op == 'muno.do_update' then
		muno.do_update(done)
	elseif a.op == 'muno.request_sync' then
		pnp.notify_sync{['audio.info']=ar_info()}
		done{result=0}
	elseif a.op == 'muno.request_event' then
		all_info(function (r)
			pnp.notify_event(r)
		end)
		done{result=0}
	else
		fail()
	end
end

radio.name2obj = function (s)
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

radio.change = function (opt)
	info('radio.change', opt)

	local to = radio.name2obj(opt.type)

	if to and radio.source ~= to then
		radio.stop()
		radio.start(to)
		prop.set('radio.default', opt.type)
	end
end

muno.stat_change = function () 
	pnp.notify_event{['muno.info']=muno.info()}
end

audio.track_stat_change = function (i)
	if i ~= 0 then return end
	local r = ar_info()
	pnp.notify_event{['audio.info']=r}
end

radio.play = function (song) 
	M.log('play', song.title)
	audio.play {
		url = song.url,
		done = function (dur) 
			info('playdone')
			radio.next{dur=dur, playdone=true}
		end
	}
end

radio.stop = function ()
	audio.stop()
end

if input then
	input.cmds = {
		[[ zpnp_notify('test') ]],
		[[ audio.setvol(audio.getvol() - 10); print(audio.getvol()) ]],
		[[ audio.setvol(audio.getvol() + 10); print(audio.getvol()) ]],
		[[ audio.setvol(80); print(audio.getvol()) ]],
		[[ audio.setvol(0); print(audio.getvol()) ]],
		[[ handle{op='audio.play_pause_toggle'} ]],
		[[ handle{op='audio.next'} ]],
		[[ gsensor_prev() ]],
		[[ gsensor_next() ]],
		[[ inputdev_on_event(33); -- keypress ]],
		[[ inputdev_on_event(1); -- vol 1 ]],
		[[ inputdev_on_event(4); -- vol 4 ]],
		[[ handle{op='radio.change_type', type='pandora'} ]],
		[[ handle{op='radio.change_type', type='local'} ]],
		[[ handle{op='radio.change_type', type='slumber'} ]],
		[[ handle{op='radio.change_type', type='douban'} ]],
		[[ handle{op='radio.change_type', type='bbcradio'} ]],
	}
end

http_server {
	addr = '127.0.0.1',
	port = 9991,
	handler = function (hr)
		local cmd = hr:body()
		local func = loadstring(cmd)
		local r, err = pcall(func)
		local s = ''
		if err then
			s = cjson.encode{err=tostring(err)}
		else
			s = cjson.encode{err=0}
		end
		hr:retjson(s)
	end,
}

http_server {
	port = 8881,
	handler = function (hr)
		local js = cjson.decode(hr:body()) or {}
		handle(js, function (r)
			hr:retjson(cjson.encode(r))
		end)
	end,
}

airplay_on_start = function ()
	airplay.start()
end

pnp = {}

pnp.notify = function () end

pnp.start = function ()
	zpnp.start()
	upnp.start()
	pnp.notify = function (r)
		zpnp.notify(r)
		upnp.notify(r)
	end
	zpnp.on_action = handle
	upnp.on_action = handle
	upnp.on_subscribe = function (a, done)
		all_info(done)
	end
end

pnp.notify_event = function (r) pnp.notify(table.add(r, {type='event'})) end
pnp.notify_sync  = function (r) pnp.notify(table.add(r, {type='sync'})) end

info('hostname', hostname())
prop.load()
airplay_start('Muno_' .. hostname())
pnp.start()

if hostplat() == 'mips' then
	require('mips')
else
	audio.setvol(50)
end

handle{op='radio.change_type', type=prop.get('radio.default', 'local')}

