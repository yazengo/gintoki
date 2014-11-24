
require('cmd')
require('audio')

am = audio.mixer()
ao = audio.out()

audio.pipe(am, ao)

play = function (url)
	if tr then
		tr.close()
	end
	local dec = audio.decoder(url)
	
	dec.probed(function ()
		info(dec.dur)
	end)

	tr = am.add(dec).done(function ()
		info('play done')
	end)
end

mix = function (url)
	local dec = audio.decoder(url)

	am.add(dec).done(function ()
		info('mix done')
	end)
end

pause = function ()
	if tr then tr.pause() end
end

resume = function ()
	if tr then tr.resume() end
end

setvol = function (v)
	am.setvol(v)
end

input.cmds = {
	[[ pause() ]],
	[[ resume() ]],
	[[ setvol(0.7) ]],
	[[ setvol(0.9) ]],
	[[ play('testaudios/10s-1.mp3') ]],
	[[ play('testaudios/2s-2.mp3') ]],
	[[ play('testaudios/2s-3.mp3') ]],
	[[ mix('testaudios/2s-1.mp3') ]],
	[[ mix('testaudios/2s-2.mp3') ]],
	[[ mix('testaudios/2s-3.mp3') ]],
}

