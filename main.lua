
require('localmusic')
require('pandora')
require('radio')
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
	end
	if radio.source ~= to then
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

ttyraw_onkey = function (key)
	if key == 'n' then
		radio.next()
	elseif key == 'p' then
		audio.pause_resume_toggle()
	end
end

on_airplay_start = function ()
	audio.play {
		url = 'airplay://',
		done = function ()
			radio.next()
		end,
	}
end

on_inputevent = function (e) 
	if e == 33 then
		info('inputdev: keypress')
		audio.pause_resume_toggle()
	end
	if e >= 0 and e <= 15 then
		local vol = math.ceil(100*e/15)
		info('inputdev: vol', e, '->', vol)
		audio.setvol(vol)
	end
end

--setloglevel(0)
upnp.start()
--audio.setvol(3)
--radio.start(pandora)
radio.start(localmusic)
--ttyraw_open(ttyraw_onkey)

