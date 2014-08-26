
require('localmusic')
require('pandora')
require('radio')
require('muno')

ar_info = function ()
	local ai = audio.info()
	local ri = radio.info()
	local r = table.add({}, ai, ri)
	return r
end

upnp.on_subscribe = function (a, done)
	done(ar_info())
end

upnp.on_action = function (a, done)
	a = a or {}
	
	info('upnp action', a)

	if a.op == 'audio.volume' then 
		vol = muno.setvol(a.value)
		done{result=vol}
	elseif a.op == 'muno.info' then
		done(muno.info())
	elseif a.op == 'audio.next' then
		radio.next()
		done{result=0}
	elseif a.op == 'audio.play_pause_toggle' then
		audio.pause_resume_toggle()
		done{result=0}
	elseif a.op == '' then
	else
		done{result=0}
	end
end

muno.on('stat_change', function () 
	info('muno stat ->', muno.info())
	upnp.notify{['muno.info']=muno.info()}
end)

audio.on('stat_change', function ()
	local r = ar_info()
	info('audio stat ->', r)
	upnp.notify{['audio.info']=r}
end)

radio.play = function (song) 
	info('play', song.title)
	audio.play{
		url = song.url,
		done = function () 
			info('playdone')
			radio.next()
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

radio.start(pandora)
--ttyraw_open(ttyraw_onkey)

