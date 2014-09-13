
audio.alert = function (url)
	if audio.is_alerting then return end
	audio.play {
		url = url,
		track = 1,
		done = function ()
			audio.is_alerting = false
		end,
	}
end

