
require('localmusic')
require('pandora')
require('bbcradio')
require('radio')
require('audio')
require('muno')

local M = {}

M.log = function (...)
	info('main:', ...)
end

ar_info = function ()
	local ai = audio.info()
	local ri = radio.info()
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

	local name = prop.get('upnp.name', 'Muno')
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

upnp.on_action = function (a, done)
	a = a or {}
	
	M.log('muno stat ->', muno.info())

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
	elseif a.op == 'audio.play' then
		if a.id then
			radio.source_setopt({id=a.id})
		end
		done{result=0}
	elseif string.hasprefix(a.op, 'local.') then
		localmusic.setopt(a, done)
	elseif string.hasprefix(a.op, 'pandora.') then
		radio.source_setopt(a, done)
	elseif string.hasprefix(a.op, 'bbcradio.') then
		radio.source_setopt(a, done)
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
		done{result=0}
	end
end

radio.change = function (opt)
	local to
	if opt.type == 'pandora' then
		to = pandora
	elseif opt.type == 'local' then
		to = localmusic
	elseif opt.type == 'bbcradio' then
		to = bbcradio
	end
	if to and radio.source ~= to then
		radio.start(to)
	end
end

muno.on('stat_change', function () 
	M.log('muno stat ->', muno.info())
	upnp.notify{['muno.info']=muno.info()}
end)

audio.on('stat_change', function ()
	local r = ar_info()
	M.log('audio stat ->', r)
	upnp.notify{['audio.info']=r}
end)

radio.play = function (song) 
	M.log('play', song.title)
	audio.play {
		url = song.url,
		done = function () 
			info('playdone')
			radio.next{playdone=true}
		end
	}
end

airplay_on_start = function ()
	audio.play {
		url = 'airplay://',
		done = function ()
			info('airplay ends')
			radio.next()
		end,
	}
end

local say_and_do = function (url, action)
	local doing
	return function ()
		if doing then return end
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

gsensor_prev = say_and_do('testaudios/prev.mp3', radio.prev)
gsensor_next = say_and_do('testaudios/next.mp3', radio.next)

on_inputevent = function (e) 
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

prop.load()
audio.setvol(50)
radio.start(localmusic)
airplay_start(prop.get('upnp.name', 'Muno') .. ' 的 Airplay')
upnp.loadconfig()
upnp.start()

