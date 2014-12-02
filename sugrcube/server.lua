
require('playlist')
require('audio')

S = {}

local function init_localmusic(done)
	if os.getenv('MUSICDIR') == nil then panic('MUSICDIR must be set') end
	local path = os.getenv('MUSICDIR')
	readdir{path, function (dirs)
		local urls = {}
		for _, d in pairs(dirs) do
			table.insert(urls, path .. '/' .. d)
		end
		S.localmusic = playlist.urls(urls).setmode('repeat_all')
		done()
	end}
end

local function init(calls, done)
	local n = 0
	for _, f in pairs(calls) do
		f(function ()
			n = n + 1
			if n == table.maxn(calls) then
				done()
			end
		end)
	end
end

init({init_localmusic, shairport.init}, function ()
	info('server starts')

	S.sw = audio.switcher()
	S.player = playlist.player().changed(function (r)
		info(r)
	end)

	audio.pipe(S.sw, audio.out())
	S.player.setsrc(S.localmusic)
	S.sw.setsrc(S.player)

	shairport.start(function (r)
		S.player.pause()
		S.sw.breakin(r)
	end)
end)

if input then
input.cmds = {
	[[ S.player.resume() ]],
	[[ S.player.pause() ]],
	[[ S.player.next() ]],
	[[ S.player.prev() ]],
	[[ S.sw.stop_breakin() ]],
}
end

