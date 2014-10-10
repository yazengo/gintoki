
require('localmusic')
require('radio')

radio.play = function (song) 
	info('play', song)
	audio.play{
		url = song.url,
		done = function () 
			info('playdone')
			radio.next()
		end
	}
end

radio.start(localmusic)

ttyraw_open(function (key)
	if key == 'n' then
		radio.next()
	elseif key == 'p' then
		audio.pause_resume_toggle()
	end
end)

ttyraw_open(function (key)
	if key == 'n' then
		radio.next()
	end
end)

