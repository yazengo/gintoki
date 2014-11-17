
require('playque')
require('muno')
require('audio')
require('burnin')
require('radio')

require('localmusic')
require('pandora')
require('douban')
require('bbcradio')

local C = {}

C.sleeping = false

--
-- airplay
-- burnin
-- peerctrl
--
C.breaking_radio = function (radio, o)
	o = o or {}
	o.done = o.done or function () end

	if o.name == 'peerctrl' and C.sleeping == true then
		return
	end

	if C.breaking_que then
		C.breaking_que.stop()
	else
		C.breaking_que = playque(2, radio)
	end

	local p = C.breaking_que

	p.on_eof = function ()
		info('closed')
		if o.resume then 
			C.radio_que.resume()
		end
		C.breaking_que = nil
	end

	p.name = o.name
	p.start()

	return p
end

C.breaking_audio = function (url, o)
	o = o or {}
	info('url', url)
	return C.breaking_radio(radio.new_songlist({url}, {loop=false}), o)
end

C.start = function ()
	C.radio_que = playque(0)
	C.radio_que.subscribe('resume', function ()
		C.sleeping = false
	end)
end

C.setopt_pre = function (o, done)
	local needstop = table.contains({
		'audio.play_pause_toggle', 'audio.pause', 'audio.resume', 
		'audio.next', 'audio.prev', 'audio.play',
		'radio.change_type',
	}, o.op)

	if C.breaking_que and o.eventsrc == 'inputdev' then
		local inputops = {
			['audio.play_pause_toggle'] = C.breaking_que.pause_resume_toggle,
			['audio.pause'] = C.breaking_que.pause,
			['audio.resume'] = C.breaking_que.resume,
		}
		local f = inputops[o.op]
		if f then 
			f()
			done{result=0}
			return true
		end
	end

	if C.breaking_que and needstop then
		C.breaking_que.stop()
	end
end

C.setopt_post = function (o)
end

muno.audioinfo = function ()
	local info = {}
	table.add(info, audio.info())
	local song = C.radio_que.song
	if not song then
		info.stat = 'fetching'
	else
		table.add(info, song)
		if info.url then info.url = nil end
	end
	return info
end

C.audio_setopt = function (o, done)
	if o.op == 'audio.volume' then 
		local vol = audio.setvol(o.value)
		muno.notify_vol_change(vol)
		done{result=vol}
		return true
	elseif o.op == 'audio.play_pause_toggle' then
		C.radio_que.pause_resume_toggle()
		done{result=0}
		return true
	elseif o.op == 'audio.pause' then
		C.radio_que.pause()
		if o.sleeping then C.sleeping = true end
		done{result=0}
		return true
	elseif o.op == 'audio.resume' then
		C.radio_que.resume()
		done{result=0}
		return true
	elseif o.op == 'audio.alert' then
		audio.alert{url=o.url}
		done{result=0}
		return true
	end
end

C.radio_setopt = function (o, done)
	if o.op == 'local.toggle_repeat_mode' then
		o.op = 'audio.toggle_repeat_mode'
	end

	info(o)

	local map = {
		['local'] = localmusic, slumber = slumbermusic, 
		pandora = pandora, bbcradio = bbcradio, douban = douban,
	}

	if o.op == 'audio.play' and R.radio_que.radio and R.radio_que.radio.setopt(o, done) then
		done{result=0}
		C.radio_que.resume()
		return true
	elseif o.op == 'audio.prev' and R.radio_que.radio and R.radio_que.radio.setopt(o, done) then
		done{result=0}
		C.radio_que.resume()
		return true
	elseif o.op == 'audio.next' then
		C.radio_que.next()
		done{result=0}
		C.radio_que.resume()
		return true
	elseif o.op == 'radio.change_type' then
		local radio = map[o.type]
		if radio and radio ~= C.radio_que.radio then
			done{result=0}
			C.radio_que.change(radio)
			C.radio_que.resume()
			return true
		end
	else
		local radio = map[string.split(o.op, '.')[1]]
		if radio and radio.setopt(o, done) then
			return true
		end
	end
end

C.misc_setopt = function (o, done)
	if o.op == 'breaking.audio' then
		C.breaking_audio(o.url, {
			name = 'inserting',
			done = function ()
				done{result=0}
			end,
			resume = o.resume,
		})
		return true
	end
end

C.setopt = function (o, done)
	local ok

	if C.setopt_pre(o, done) then
		ok = true
	end

	if not ok then
		if C.audio_setopt(o, done) then
			ok = true
		elseif C.radio_setopt(o, done) then
			ok = true
		elseif burnin.setopt(o, done) then
			ok = true
		elseif C.misc_setopt(o, done) then
			ok = true
		end
	end

	C.setopt_post(o)

	return ok
end

ctrl = C

