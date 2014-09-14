
audio.alert = function (url, vol)
	if audio.is_alerting then return end
	audio.setopt{ track0_setvol = true, vol = vol }
	audio.play {
		url = url,
		track = 1,
		done = function ()
			audio.is_alerting = false
			audio.setopt{ track0_setvol = false }
		end,
	}
end

