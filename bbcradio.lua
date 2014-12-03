
require('playlist')

local function loadlist ()
	local js = loadjson('bbcradio.json') or {}
	local r = {}
	for k, v in pairs(js.radios or {}) do
		local s = {
			id = tostring(k),
			name = v.name,
			url = v.uri,
		}
		r[k] = s
	end
	return r
end

local B = playlist.songs(loadlist()).setmode('repeat_all')

B.setopt_stations_list = function (o, done)
	done{stations=B.songs}
end

B.setopt_station_choose = function (o, done)
	B.jump_to(tonumber(o.id))
	done()
end

B.name = 'bbcradio'

bbcradio = B

