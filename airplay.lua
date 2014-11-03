
local A = {}

A.stop = function ()
	audio.stop{track=2}
	A.on_stop()
end

A.on_stop = function ()
	radio.disabled = false
	A.enabled = false
end

A.setopt = function (a, done)
	if not A.enabled then 
		return
	end

	if a.op == 'audio.play_pause_toggle' then
		if a.current then
			audio.pause_resume_toggle{track=2}
			done{result=0}
			return true
		else
			A.stop()
		end
	elseif a.op == 'audio.pause' then
		if a.current then 
			audio.pause{track=2}
			done{result=0}
			return true
		else
			A.stop()
		end
	elseif a.op == 'audio.resume' then
		if a.current then 
			audio.resume{track=2}
			done{result=0}
			return true
		else
			A.stop()
		end
	elseif a.op == 'radio.change_type' then
		A.stop()
	elseif a.op == 'audio.prev' then
		A.stop()
	elseif a.op == 'audio.next' then
		A.stop()
	elseif a.op == 'audio.play' then
		A.stop()
	end
end

A.on_start = function ()
	radio.disabled = true
	A.enabled = true

	audio.pause()
	audio.play {
		url = 'airplay://',
		track = 2,
		done = function ()
			A.on_stop()
		end,
	}
end

airplay_on_start = function ()
	A.on_start()
end

A.start = function ()
	popen {
		cmd = 'which shairport',
		done = function (r, code)
			if code == 0 then
				airplay_start('Muno_' .. hostname())
			end
		end,
	}
end

airplay = A

