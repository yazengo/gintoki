
require('localmusic')
require('pandora')
require('douban')
require('bbcradio')
require('airplay')
require('radio')
require('audio')
require('muno')
require('upnp')

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

upnp.loadconfig = function ()
	local tpl = io.open('upnpweb/munodevicedesc.tpl.xml', 'r')
	if not tpl then error('upnp tpl open failed') end
	local xml = io.open('upnpweb/munodevicedesc.xml', 'w+')
	if not xml then error('upnp xml open failed') end

	local name = hostname()
	local s = tpl:read('*a')
	s = string.gsub(s, '{NAME}', name)
	xml:write(s)

	info('upnp.name', name)

	tpl:close()
	xml:close()
end

upnp.on_subscribe = function (a, done)
	done(all_info())
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
	elseif a.op == 'audio.request_info' then
		upnp.notify{['audio.info']=ar_info()}
		done{result=0}
	else
		fail()
	end
end

upnp.on_action = handle

radio.name2obj = function (s)
	if s == 'pandora' then
		return pandora
	elseif s == 'local' then
		return localmusic
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
	upnp.notify{['muno.info']=muno.info()}
end

audio.track_stat_change = function (i)
	if i ~= 0 then return end
	local r = ar_info()
	upnp.notify{['audio.info']=r}
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

local say_and_do = function (url, k, action)
	local doing
	return function ()
		if not (radio.source and radio.source[k]) then
			return 
		end
		if doing then return end
		handle{op='audio.pause'}
		doing = true
		audio.play {
			url = url,
			done = function ()
				doing = nil
				action()
			end,
		}
	end
end

gsensor_prev = say_and_do('testaudios/prev.mp3', 'prev', function () handle{op='audio.prev'} end)
gsensor_next = say_and_do('testaudios/next.mp3', 'next', function () handle{op='audio.next'} end)

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
		audio.alert('testaudios/hello-muno.mp3', 0)
	end

	-- network up
	if e == 36 then
		audio.alert('testaudios/connected.mp3', 20)
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
		[[ audio.setvol(audio.getvol() - 10); print(audio.getvol()) ]],
		[[ audio.setvol(audio.getvol() + 10); print(audio.getvol()) ]],
		[[ audio.setvol(80); print(audio.getvol()) ]],
		[[ audio.setvol(0); print(audio.getvol()) ]],
		[[ handle{op='audio.play_pause_toggle'} ]],
		[[ handle{op='audio.next'} ]],
		[[ gsensor_next() ]],
		[[ handle{op='radio.change_type', type='pandora'} ]],
		[[ handle{op='radio.change_type', type='local'} ]],
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

info('hostname', hostname())
prop.load()
audio.setvol(50)
airplay_start(hostname() .. ' 的 Airplay')
upnp.loadconfig()
upnp.start()
if inputdev_init then inputdev_init() end

handle{op='radio.change_type', type=prop.get('radio.default', 'local')}

