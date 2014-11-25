
require('player')
require('audio')
require('cmd')

S = {}

S.urls = {
	'testaudios/2s-1.mp3',
	'testaudios/2s-2.mp3',
	'testaudios/2s-3.mp3',
}
S.at = 1

S.fetch = function (done)
	set_timeout(function ()
		done{url=S.urls[S.at]}
		S.at = S.at + 1
		local n = table.maxn(S.urls) 
		if S.at > n then
			S.stopped_cb()
		end
	end, 1000)
end

S.cancel_fetch = function ()
end

p = player(audio.out()).setsrc(S)

input.cmds = {
	[[ S.skip() ]],
	[[ S.stop() ]],
}

