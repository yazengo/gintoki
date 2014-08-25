
require('localmusic')
require('radio')
require('muno')

upnp.on('action', function (a)
	a = a or {}
	
	info('upnp action', a)

	if a.op == 'audio.volume' then 
		vol = muno.setvol(a.value)
		return {result=vol} 
	elseif a.op == 'audio.next' then
		radio.next()
		return {result=0}
	elseif a.op == 'audio.prev' then
		radio.prev()
		return {result=0}
	elseif a.op == 'audio.play_pause_toggle' then
		audio.pause_resume_toggle()
		return {result=0}
	end

	return {result=0}
end)

muno.on('stat_change', function () 
	info('muno stat ->', muno.info())
	upnp.notify{['muno.info']=muno.info()}
end)

audio.on('stat_change', function ()
	local ai = audio.info()
	local ri = radio.info()
	local r = table.add({}, ai, ri)
	info('audio stat ->', r)
	upnp.notify{['audio.info']=r}
end)

radio.on('play', function (song) 
	info('play', song)
	audio.play{
		url = song.url,
		done = function () 
			radio.next()
		end
	}
end)

radio.start()

ttyraw_open(function (key)
	if key == 'n' then
		radio.next()
	elseif key == 'p' then
		audio.pause_resume_toggle()
	end
end)

