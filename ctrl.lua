
require('radio')
require('playque')
require('muno')
require('audio')

local C = {}

C.radio_que = playque(0, radio)

C.breaking_n = 0

C.breaking_enter = function (o)
	info('pause', C.breaking_n)
	C.radio_que.pause()
end

C.breaking_leave = function (o)
	info('resume', C.breaking_n)
	if o.resume then
		C.radio_que.resume()
	end
end

C.breaking_radio = function (radio, o)
	o.done = o.done or function () end

	if C.breaking_que then
		C.breaking_que.stop()
	end

	local p = playque(2, radio)

	p.on_closed = function ()
		info('closed')
		o.done()
		C.breaking_n = C.breaking_n - 1
		if C.breaking_n == 0 then C.breaking_leave(o) end
		if p == C.breaking_que then
			C.breaking_que = nil
		end
	end

	p.name = o.name
	p.start()

	C.breaking_n = C.breaking_n + 1
	C.breaking_que = p
	if C.breaking_n == 1 then C.breaking_enter(o) end

	return p
end

C.breaking_audio = function (url, o)
	info('url', url)
	C.breaking_radio(radio.new_songlist({url}, {loop=false}), o)
end

C.start = function ()
	C.radio_que.start()
end

C.setopt_pre = function (o)
end

C.setopt_post = function (o)
	if o.op == 'audio.play' or 
		 o.op == 'audio.next' or
		 o.op == 'audio.prev' 
	then
		info('resume')
		C.radio_que.resume()
	end
end

C.setopt = function (o, done)
	C.setopt_pre(o)

	local ok

	if o.op == 'audio.volume' then 
		local vol = audio.setvol(o.value)
		muno.notify_vol_change(vol)
		done{result=vol}
		ok = true
	elseif o.op == 'audio.play_pause_toggle' then
		C.radio_que.pause_resume_toggle()
		done{result=0}
		ok = true
	elseif o.op == 'audio.pause' then
		C.radio_que.pause()
		done{result=0}
		ok = true
	elseif o.op == 'audio.resume' then
		C.radio_que.resume()
		done{result=0}
		ok = true
	elseif o.op == 'audio.alert' then
		audio.alert{url=o.url}
		done{result=0}
		ok = true
	elseif o.op == 'breaking.audio' then
		C.breaking_audio(o.url, {
			name = 'inserting',
			done = function ()
				done{result=0}
			end,
			resume = o.resume,
		})
		ok = true
	elseif radio.setopt(o, done) then
		ok = true
	end

	C.setopt_post(o)

	return ok
end

ctrl = C

