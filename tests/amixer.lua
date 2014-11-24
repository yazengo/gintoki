
require('audio')
require('pcopy')

function decoder()
	return pexec('avconv -i testaudios/10s-1.mp3 -f s16le -ar 44100 -ac 2 - | head -c 65536', 'r')
end

m = audio.mixer()
t = m.add(decoder())
t = m.add(decoder())

t.done(function ()
	info('playend')
end)

pcopy(m, aout()).done(function ()
	info('alldone')
end)

