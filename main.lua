
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

all_info = function ()
	return {
		['audio.info']=ar_info(),
		['muno.info']=muno.info(),
	}
end

handle = function (a, done)
	done = done or function () end
	local fail = function () done{result=1, msg='params invalid'} end
	
	if not a or not a.op or not isstr(a.op) then
		fail()
		return
	end

	if airplay.setopt(a, done) then
		return
	end

	if a.op == 'audio.play' then
		a.op = 'local.play'
	end

	if a.op == 'audio.volume' then 
		local vol = muno.setvol(a.value)
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
		pnp_notify_sync{['audio.info']=ar_info()}
		done{result=0}
	elseif a.op == 'muno.request_event' then
		pnp_notify_event(all_info())
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
	pnp_notify_event{['muno.info']=muno.info()}
end

audio.track_stat_change = function (i)
	if i ~= 0 then return end
	local r = ar_info()
	pnp_notify_event{['audio.info']=r}
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

local say_and_do = function (k)
	return function ()
		if not (radio.source and radio.source[k]) and k == 'prev' then
			k = 'next'
		end
		audio.alert {
			url = 'testaudios/' .. k .. '.mp3',
			done = function ()
				handle { op = 'audio.' .. k}
			end,
		}
	end
end

gsensor_prev = say_and_do('prev')
gsensor_next = say_and_do('next')

inputdev_on_event = function (e) 
	info('e=', e)

	if e == 33 then
		info('inputdev: keypress')
		audio.pause_resume_toggle()
	end

	if e == 38 then
		info('inputdev: volend')
		audio.setvol(0)
	end

	if e == 332 then
		info('long press')
		audio.alert {
			url = 'testaudios/hello-muno.mp3',
			vol = 0,
		}
	end

	-- network up
	if e == 36 then
		audio.alert {
			url = 'testaudios/connected.mp3',
			vol = 20,
		}
		info('network up')
		upnp.start()
	end

	-- network down
	if e == 37 then
		info('network down')
		upnp.stop()
	end

	if e == 40 then
		gsensor_next()
	end

	if e == 41 then
		gsensor_prev()
	end

	if e >= 0 and e <= 15 then
		local vol = math.ceil(100*e/15)
		info('inputdev: vol', e, '->', vol)
		audio.setvol(vol)
	end
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

upnp.on_subscribe = function (a, done)
	done(all_info())
end

zpnp.on_action = handle
upnp.on_action = handle

pnp_notify = function (r)
	zpnp.notify(r)
	upnp.notify(r)
end

pnp_start = function ()
	zpnp.start()
	upnp.start()
end

pnp_notify_event = function (r) pnp_notify(table.add(r, {type='event'})) end
pnp_notify_sync  = function (r) pnp_notify(table.add(r, {type='sync'})) end

info('hostname', hostname())
prop.load()
audio.setvol(50)
airplay_start('Muno_' .. hostname())
pnp_start()
if inputdev_init then inputdev_init() end

handle{op='radio.change_type', type=prop.get('radio.default', 'local')}

