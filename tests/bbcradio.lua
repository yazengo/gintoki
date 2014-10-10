
-- setloglevel(0)
--P.verbose = 1

--P.cookie.station_id = '2178401799297707427'

if input then
	input.cmds = {
		[[ bbcradio.setopt({op='bbcradio.stations_list'}, info) ]],
		[[ bbcradio.setopt({op='bbcradio.station_choose', id=argv[2]}, info) ]],
		[[ info(bbcradio.next()) ]],
	}
end

