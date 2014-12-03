
require('playlist')

local L = playlist.urls().setmode('repeat_all')
local S = playlist.urls().setmode('repeat_all')

local function init (list, env, done)
	if os.getenv(env) == nil then 
		done()
		return
	end

	local path = os.getenv(env)
	readdir {path, function (dirs)
		local urls = {}
		for _, d in pairs(dirs) do
			table.insert(urls, path .. '/' .. d)
		end
		list.seturls(urls)
		done()
	end}
end

L.name = 'local'

L.init = function (done)
	init(L, 'MUSICDIR', done)
end

S.name = 'slumber'

S.init = function (done)
	init(L, 'SLUMBERDIR', done)
end

L.setopt_songs_list = function (o, done)
	local r = {}
	for k, v in pairs(L.urls) do
		table.insert(r, {id=k, title=basename(v)})
	end
	done(r)
end

L.set_play_mode = function (o, done)
	if type(o.mode) ~= 'string' then
		panic('invalid mode')
	end
	L.setmode(o.mode)
	done()
end

L.toggle_repeat_mode = function (o, done)
	if L.mode == 'repeat_all' then
		L.setmode('repeat_one')
	elseif L.mode == 'repeat_one' then
		L.setmode('repeat_all')
	end
	done()
end

localmusic = L
slumbermusic = S

