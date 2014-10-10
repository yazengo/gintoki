
audio.alert = function (url, vol)
	if audio.is_alerting then return end

	audio.setfilter { 
		enabled = true,
		slot = 0,
		type = 'highlight',
		i = 1,
		vol = vol,
	}

	audio.play {
		url = url,
		track = 1,
		done = function ()
			audio.is_alerting = false
			audio.setfilter { enabled = false, slot = 0 }
		end,
	}
end

