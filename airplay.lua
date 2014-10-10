
local A = {}

A.stop = function ()
	audio.stop{track=2}
end

A.done = function ()
	info('done')
	if radio.source == A then
		radio.source = A.source_prev
	end
end

A.setopt = function (a, done)
	if a.op == 'audio.play_pause_toggle' then
		audio.pause_resume_toggle{track=2}
		done{result=0}
		return true
	elseif a.op == 'audio.pause' then
		audio.pause{track=2}
		done{result=0}
		return true
	elseif a.op == 'audio.resume' then
		audio.resume{track=2}
		done{result=0}
		return true
	elseif a.op == 'radio.change_type' then
		A.stop()
	elseif a.op == 'audio.prev' then
		A.stop()
		radio.source = A.source_prev
	elseif a.op == 'audio.next' then
		A.stop()
		radio.source = A.source_prev
	end
end

A.next = function ()
end

A.prev = function ()
end

A.start = function (source_prev)
	A.source_prev = source_prev
	audio.pause()
	audio.play {
		url = 'airplay://',
		track = 2,
		done = A.done,
	}
end

A.info = function ()
	return A.source_prev.info()
end

airplay = A

