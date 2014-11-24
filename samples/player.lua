
require('cmd')
require('audio')
require('pipe')

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

	tr, tr_p = am.add()
	pipe.copy(dec[1], tr_p).done(function ()
		info('play done')
	end)
end

playseq = function (...)
	local tr, tr_p = am.add()
	local urls = {...}
	local i = 1

	local function loop()
		local url = urls[i]
		i = i + 1
		if not url then return end
		local dec = audio.decoder(url)
		pipe.copy(dec[1], tr_p, 'bw').done(function ()
			loop()
		end)
	end

	loop()
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
	[[ playseq('testaudios/2s-1.mp3', 'testaudios/2s-2.mp3', 'testaudios/2s-3.mp3') ]],
	[[ mix('testaudios/2s-1.mp3') ]],
	[[ mix('testaudios/2s-2.mp3') ]],
	[[ mix('testaudios/2s-3.mp3') ]],
}

