
require('audio')

loop = function ()
	audio.pipe(pexec('cat ~/shairport/audio', 'r'), audio.out())
end

loop()

