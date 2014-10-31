
audio.alert = function (o)
	if audio.is_alerting then return end
	audio.is_alerting = true

	if not o.vol then o.vol = 0 end

	if o.fadeothers == nil then
		o.fadeothers = true
	end

	audio.setfilter { 
		enabled = true,
		slot = 0,
		type = 'highlight',
		i = 1,
		vol = o.vol,
		fadeothers = o.fadeothers,
	}

	audio.play {
		url = o.url,
		track = 1,
		done = function ()
			audio.is_alerting = false
			audio.setfilter { enabled = false, slot = 0 }
			if o.done then
				o.done()
			end
		end,
	}
end

