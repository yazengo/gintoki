
require('pandora')
require('cmd')

local P = pandora

-- setloglevel(0)
--P.verbose = 1
P.start()

--P.cookie.station_id = '2178401799297707427'

if input then
	input.cmds = {
		[[ pandora.test_login() ]],
		[[ info(pandora.info()) ]],
		[[ info(pandora.next()) ]],
		[[ pandora.setopt{op='pandora.login', username='enliest@qq.com', password='enliest1653'} ]],
		[[ pandora.setopt{op='pandora.login', username='cfanfrank@gmail.com', password='enliest1653'} ]],
		[[ pandora.setopt{op='pandora.login', username='fake', password='fake'} ]],
		[[ pandora.setopt{op='pandora.genres_list'} ]],
		[[ pandora.setopt({op='pandora.stations_list'}, info) ]],
		[[ pandora.setopt{op='pandora.songs_list'} ]],
		[[ pandora.setopt{op='pandora.genre_choose', id=argv[2]} ]],
		[[ pandora.setopt{op='pandora.station_choose', id=argv[2]} ]],
		[[ pandora.add_feedback(table.add(pandora.cookie, {song_id=pandora.songs[1].id, like=true}), info) ]],
		[[ pandora.add_feedback(table.add(pandora.cookie, {song_id=pandora.songs[2].id, like=true}), info) ]],
	}
end

