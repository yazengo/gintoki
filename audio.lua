
audio.alert = function (url)
	if audio.is_alerting then return end
	audio.setopt{ track0_vol20 = true }
	audio.play {
		url = url,
		track = 1,
		done = function ()
			audio.is_alerting = false
			audio.setopt{ track0_vol20 = false }
		end,
	}
end

