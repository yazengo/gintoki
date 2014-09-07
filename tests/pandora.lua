
--setloglevel(0) 

local encode_params = function (p)
	local r = {}
	for k,v in pairs(p) do
		table.insert(r, k .. '=' .. urlencode(v))
	end
	return table.concat(r, '&')
end

local savejson = function (fname, js) 
	local f = io.open(fname, 'w+')
	f:write(cjson.encode(js))
	f:close()
end

local loadjson = function (fname) 
	local f = io.open(fname, 'r')
	local s = f:read('*a')
	f:close()
	return cjson.decode(s) or {}
end

local P = {}

P.url = '://tuner.pandora.com/services/json/?'

P.log = logger('pandora')

P.cookies = {}

P.bf_encode = blowfish('6#26FRL$ZWD')
P.bf_decode = blowfish('R=U!LH$O2B#')

P.loadcookie = function ()
	P.cookie = loadjson('pandora.json')
end

P.savecookie = function ()
	P.log('cookie saved', P.cookie)
	savejson('pandora.json', P.cookie)
end

local totable = function (t)
	if not t or type(t) ~= 'table' then
		return {}
	end
	return t
end

local istable = function (s)
	if not s or type(s) ~= 'table' then
		return false
	end
	return true
end

local tostr = function (s)
	if not s or type(s) ~= 'string' then
		return ''
	end
	return s
end

local isstr = function (s)
	if not s or type(s) ~= 'string' then
		return false
	end
	return true
end

P.change_stat = function (stat)
	P.log(P.stat, '->', stat)
	P.stat = stat
end

P.call = function (p)

	p.data = p.data or {}
	p.proto = p.proto or 'https'

	local url = p.proto .. P.url .. encode_params(p.params)
	local log = logger('pandora_api')

	local reqstr = cjson.encode(p.data)
	if p.blowfish then
		reqstr = P.bf_encode:encode_hex(reqstr)
	end

	log(url, reqstr)

	return curl {
		proxy = 'localhost:8888',
		url = url,
		content_type = 'text/plain',
		user_agent = 'pithos',
		reqstr = reqstr,
		done = function (r) 
			log(r)
			r = cjson.decode(r) or {}
			if r.stat ~= 'ok' or type(r.result) ~= 'table' then
				p.done(nil, r)
			else
				p.done(r.result, r)
			end
		end,
	}
end

P.cancel_task = function (done)
	if P.task then
		P.task.cancel(done)
	else
		done()
	end
end

P.decode_synctime = function (s)
	local tm = P.bf_decode:decode_hex(s)
	local r = ''
	for i = 5,14 do
		local ch = string.byte(tm, i)
		if not ch then break end
		r = r .. string.char(ch)
	end
	return r
end

P.partner_login = function (cookie, done)
	info('partner login')

	return P.call {
		proto = 'https',
		blowfish = false,

		params = { method = 'auth.partnerLogin' },
		data = {
			deviceModel = 'android-generic',
			username = 'android',
			password = 'AC7IBG09A3DTSYM4R41UJWL07VLN8JI7',
			version = '5',
		},
		done = function (r, stat)
			if not r then
				done(nil, stat)
			end

			r.syncTime = tostr(r.syncTime)
			r.partnerId = tostr(r.partnerId)
			r.partnerAuthToken = tostr(r.partnerAuthToken)

			done({
				partner_time_offset = P.decode_synctime(r.syncTime) - os.time(),
				partner_id = r.partnerId,
				partner_auth_token = r.partnerAuthToken,
			}, stat)
		end,
	}
end

P.user_login = function (cookie, done)
	local log = logger('user_login')

	log('time_offset', cookie.partner_time_offset)

	return P.call {
		proto = 'https',
		blowfish = true,

		params = { 
			method = 'auth.userLogin',
			partner_id = cookie.partner_id,
			auth_token = cookie.partner_auth_token,
		},
		data = {
			syncTime = os.time() + cookie.partner_time_offset,
			partnerAuthToken = cookie.partner_auth_token,
			loginType = 'user',
			username = cookie.username,
			password = cookie.password,
		},
		done = function (r, stat)
			if not r then
				done(nil, stat)
				return
			end

			r.userId = tostr(r.userId)
			r.userAuthToken = tostr(r.userAuthToken)

			done({
				user_id = r.userId,
				user_auth_token = r.userAuthToken,
			}, stat)
		end,
	}
end

P.grep_songs = function (old)
	local r = {}

	local grep = function (s)
		if not istable(s.audioUrlMap) or 
			 not istable(s.audioUrlMap.highQuality) or
			 not s.audioUrlMap.highQuality.audioUrl
		then
			return nil
		end
		return {
			url = s.audioUrlMap.highQuality.audioUrl,
			title = s.songName,
			album = s.albumName,
			artist = s.artistName,
			id = s.trackToken,
			cover_url = s.albumArtUrl,
		}
	end

	for _, old_s in pairs(totable(old.items)) do
		local s = grep(old_s)
		if s then table.insert(r, s) end
	end

	if table.maxn(r) == 0 then r = nil end
	return r
end

P.grep_stations = function (old)
	local r = {}
	for _, s in pairs(totable(old.stations)) do
		if isstr(s.stationId) and isstr(s.stationName) then
			table.insert(r, {id=s.stationId, name=s.stationName})
		end
	end

	if table.maxn(r) == 0 then r = nil end
	return r
end

P.grep_genres = function (old)
	local r = {}

	for _, g in pairs(totable(old.categories)) do
		local slist = {}
		for _, s in pairs(totable(g.stations)) do
			if isstr(s.stationId) and isstr(s.stationName) then
				table.insert(slist, {id=s.stationId, name=s.stationName})
			end
		end
		if table.maxn(slist) > 0 then
			table.insert(r, {stations=slist, name=g.categoryName})
		end
	end

	if table.maxn(r) == 0 then r = nil end
	return r
end

P.choose_genres_done = function (r, stat)
	if not r or not r.stationId then
		P.change_stat('need_station_id')
		return
	end

	P.cookie.station_id = r.stationId
	P.savecookie()

	P.log('station_id ->', P.cookie.station_id)
	P.change_stat('fetching_songlist')
	P.task = P.songs_list(P.cookie, P.songs_list_done)
end

P.choose_genres = function (cookie, id, done)
	return P.call {
		proto = 'http',
		blowfish = true,
		params = {
			method = 'station.createStation',
			user_id = cookie.user_id,
			partner_id = cookie.partner_id,
			auth_token = cookie.user_auth_token,
		},
		data = {
			syncTime = os.time() + cookie.partner_time_offset,
			userAuthToken = cookie.user_auth_token,
			musicToken = id,
		},
		done = function (r, stat) 
			done(r, stat)
		end,
	}
end

P.stations_list = function (cookie, done)
	return P.call {
		proto = 'http',
		blowfish = true,
		params = {
			method = 'user.getStationList',
			user_id = cookie.user_id,
			partner_id = cookie.partner_id,
			auth_token = cookie.user_auth_token,
		},
		data = {
			syncTime = os.time() + cookie.partner_time_offset,
			userAuthToken = cookie.user_auth_token,
		},
		done = function (r, stat) 
			if r then r = P.grep_stations(r) end
			done(r, stat)
		end,
	}
end

P.genres_list = function (cookie, done)
	return P.call {
		proto = 'http',
		blowfish = true,
		params = {
			method = 'station.getGenreStations',
			user_id = cookie.user_id,
			partner_id = cookie.partner_id,
			auth_token = cookie.user_auth_token,
		},
		data = {
			syncTime = os.time() + cookie.partner_time_offset,
			userAuthToken = cookie.user_auth_token,
		},
		done = function (r, stat) 
			if r then r = P.grep_genres(r) end
			done(r, stat)
		end,
	}
end

P.songs_list = function (cookie, done)
	return P.call {
		proto = 'https',
		blowfish = true,
		params = {
			method = 'station.getPlaylist',
			user_id = cookie.user_id,
			partner_id = cookie.partner_id,
			auth_token = cookie.user_auth_token,
		},
		data = {
			stationToken = cookie.station_id,
			syncTime = os.time() + cookie.partner_time_offset,
			userAuthToken = cookie.user_auth_token,
		},
		done = function (r, stat)
			if r then r = P.grep_songs(r) end
			done(r, stat)
		end,
	}
end

P.partner_login_done = function (r, stat)
	P.task = nil

	if not r then
		P.change_stat('auth_partner_failed')
	else
		table.add(P.cookie, r)
		P.savecookie()

		P.change_stat('auth_user')
		P.task = P.user_login(P.cookie, P.user_login_done)
	end
end

P.user_login_done = function (r, stat)
	P.task = nil

	if r then
		table.add(P.cookie, r)
		P.savecookie()

		if not P.cookie.station_id then
			P.change_stat('need_station_id')
		else
			P.change_stat('fetching_songlist')
			P.task = P.songs_list(P.cookie, P.songs_list_done)
		end
	else
		if stat.code == 1002 then
			P.change_stat('auth_user_failed')
		else
			P.change_stat('auth_partner')
			P.task = P.partner_login(P.cookie, P.partner_login_done)
		end
	end
end

P.songs_list_done = function (r, stat)
	P.task = nil

	if r then
		P.change_stat('songlist_ready')
	else
		if stat.code == 1006 then
			P.change_stat('need_station_id')
		elseif stat.code == 1001 or stat.code == 1004 then
			P.change_stat('auth_user')
			P.task = P.user_login(P.cookie, P.user_login_done)
		else
			P.change_stat('server_error')
		end
	end
end

P.init = function ()
	P.cookie = loadjson('pandora.json')

	local c = P.cookie

	P.log('cookies', c)

	if c.username and c.password and
	   c.partner_auth_token and c.user_auth_token and 
		 c.station_id
	then
		P.change_stat('fetching_first_songlist')
		P.task = P.songs_list(c, P.songs_list_done)
		return
	end

	if c.username and c.password and 
		 c.partner_auth_token and c.user_auth_token
	then
		P.change_stat('need_station_id')
		return
	end

	if c.username and c.password then
		P.change_stat('auth_partner')
		P.task = P.partner_login(c, P.partner_login_done)
		return
	end

	P.stat = 'need_user_pass'
end

--[[

need_user_pass
need_station_id
auth_partner
auth_partner_failed
auth_user
auth_user_failed
fetching_first_songlist
fetching_songlist
songlist_ready
server_error

fetching_songlist -> E1001 -> auth_user
fetching_songlist -> E1006 -> need_station_id
fetching_songlist -> E -> server_error
fetching_songlist -> songlist_ready

auth_partner -> auth_partner_failed
auth_partner -> auth_user

auth_user -> auth_user_failed
auth_user -> fetching_songlist

]]--

P.info = function ()
	-- auth_ok/auth_failed/need_auth/auth_processing
	local map = {
		need_user_pass = 'need_auth',
		need_station_id = 'need_auth',
		auth_partner = 'auth_processing',
		auth_partner_failed = 'auth_failed',
		auth_user = 'auth_processing',
		auth_user_failed = 'auth_failed',
		fetching_first_songlist = 'auth_processing',
		fetching_songlist = 'auth_ok',
		choosing_genres = 'auth_ok',
		songlist_ready = 'auth_ok',
		server_error = 'need_auth',
	}
	return {
		stat = map[P.stat],
		stat_detail = P.stat,
	}
end

P.setopt = function (o, done)
	done = done or function () end

	P.log('setopt', o)

	if o.op == 'pandora.genre_choose' then
		P.cancel_task(function ()
			P.change_stat('choosing_genres')
			P.choose_genres(P.cookie, o.id, function (r, stat)
				done{result=0}
				P.choose_genres_done(r, stat)
			end)
		end)
	elseif o.op == 'pandora.station_choose' then
		P.cancel_task(function ()
			P.change_stat('fetching_songlist')
			P.cookie.station_id = o.id
			P.savecookie()
			P.songs_list(P.cookie, o.id, P.songs_list_done)
		end)
	elseif o.op == 'pandora.login' then
		P.cancel_task(function ()
			P.change_stat('auth_user')
			P.cookie.username = o.username
			P.cookie.password = o.password
			P.savecookie()
			P.partner_login(P.cookie, P.partner_login_done)
		end)
	elseif o.op == 'pandora.genres_list' then
		P.genres_list(P.cookie, function (r, stat)
			done(r)
		end)
	elseif o.op == 'pandora.stations_list' then
		P.stations_list(P.cookie, function (r, stat)
			done(r)
		end)
	end
end

P.songs = {}
P.songs_i = 0

pandora = P

P.init()
--P.cookie.station_id = '2178401799297707427'

if input then
	input.cmds = {
		[[ pandora.setopt{op='pandora.login', username='enliest@qq.com', password='enliest1653'} ]],
		[[ pandora.setopt{op='pandora.login', username='cfanfrank@gmail.com', password='enliest1653'} ]],
		[[ pandora.setopt({op='pandora.genres_list'}) ]],
		[[ pandora.setopt({op='pandora.stations_list'}) ]],
		[[ pandora.setopt{op='pandora.genre_choose', id=arg1} ]],
		[[ pandora.setopt{op='pandora.station_choose', id=arg1} ]],
	}
end

