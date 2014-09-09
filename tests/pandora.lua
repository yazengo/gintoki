

local P = pandora

P.verbose = 1
P.cookie = P.loadcookie()
P.start()

--P.cookie.station_id = '2178401799297707427'

if input then
	input.cmds = {
		[[ info(pandora.info()) ]],
		[[ info(pandora.next()) ]],
		[[ pandora.setopt{op='pandora.login', username='enliest@qq.com', password='enliest1653'} ]],
		[[ pandora.setopt{op='pandora.login', username='cfanfrank@gmail.com', password='enliest1653'} ]],
		[[ pandora.setopt{op='pandora.genres_list'} ]],
		[[ pandora.setopt{op='pandora.stations_list'} ]],
		[[ pandora.setopt{op='pandora.songs_list'} ]],
		[[ pandora.setopt{op='pandora.genre_choose', id=argv[2]} ]],
		[[ pandora.setopt{op='pandora.station_choose', id=argv[2]} ]],
	}
end

