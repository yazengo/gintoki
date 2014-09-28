
info('-- test audio mixer on done --')

i = 0
audio.play{
	url = 'testaudios/10s-2.mp3',
	done = function () 
		info('done')
	end,
	i=0,
}

audio.on('stat_change', function () 
	info('change->', audio.info())
end)

vol = 100

poll = function ()
	audio.setvol(vol)
	audio.pause_resume_toggle() 
	info(audio.info(), audio.getvol())
	vol = vol - 10
	set_timeout(poll, 1000)
end

poll()

