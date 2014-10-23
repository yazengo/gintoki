
local A = {}

A.stop = function ()
	audio.stop{track=2}
	radio.disabled = false
end

A.setopt = function (a, done)
	if a.op == 'audio.play_pause_toggle' then
		A.stop()
	elseif a.op == 'audio.resume' then
		A.stop()
		return true
	elseif a.op == 'radio.change_type' then
		A.stop()
	elseif a.op == 'audio.prev' then
		A.stop()
	elseif a.op == 'audio.next' then
		A.stop()
	elseif a.op == 'local.play' then
		A.stop()
	end
end

A.start = function ()
	radio.disabled = true
	audio.pause()
	audio.play {
		url = 'airplay://',
		track = 2,
	}
end

airplay = A

