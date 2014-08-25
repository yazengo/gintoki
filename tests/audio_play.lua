
info('-- test audio mixer on done --')

i = 0
audio.play{
	url = 'testdata/10s-2.mp3',
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
	if vol == 80 then audio.pause_resume_toggle() end
	if vol == 70 then audio.pause_resume_toggle() end
	info(audio.info(), audio.getvol())
	vol = vol - 10
	set_timeout(poll, 1000)
end

poll()

