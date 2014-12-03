
require('prop')
require('playlist')

local P = {}

P.url = '://tuner.pandora.com/services/json/?'

P.bf_encode = blowfish('6#26FRL$ZWD')
P.bf_decode = blowfish('R=U!LH$O2B#')

P.loadcookie = function ()
	return prop.get('pandora.cookie', {})
end

P.savecookie = function (c)
	info('cookie saved', c)
	prop.set('pandora.cookie', c)
end

P.setcookie = function (c)
	P.cookie = c
	P.savecookie(c)
end

P.call = function (p)
	local v = false

	p.data = p.data or {}
	p.proto = p.proto or 'https'

	if v then info(p.params, p.data) end

	local url = p.proto .. P.url .. encode_params(p.params)

	local reqstr = cjson.encode(p.data)
	if p.blowfish then
		reqstr = P.bf_encode:encode_hex(reqstr)
	end

	return curl {
		retry = 1000,
		proxy = prop.get('gfw.exist', true) and 'sugrsugr.com:8889',
		url = url,
		content_type = 'text/plain',
		user_agent = 'pithos',
		reqstr = reqstr,
		done = function (r, st) 
			if not r then
				p.done()
				return
			end

			r = cjson.decode(r) or {}
			if r.stat ~= 'ok' or type(r.result) ~= 'table' then
				p.done(nil, r)
			else
				p.done(r.result, r)
			end
		end,
	}
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

P.data_user = function (c, p)
	return table.add(p, {
		syncTime = os.time() + c.partner_time_offset,
		userAuthToken = c.user_auth_token,
		auth_token = c.user_auth_token,
	})
end

P.params_user = function (c, p)
	return table.add(p, {
		user_id = c.user_id,
		partner_id = c.partner_id,
		auth_token = c.user_auth_token,
	})
end

P.user_cookie_keys = {
	'user_id', 'user_auth_token',
	'partner_id', 'partner_time_offset', 'partner_auth_token'
}

P.has_all_user_cookie = function (c)
	for _,k in ipairs(P.user_cookie_keys) do
		if not c[k] then return false end
	end
	return true
end

P.clean_user_cookie = function (c)
	for _,k in ipairs(P.user_cookie_keys) do
		c[k] = nil
	end
end

P.partner_login = function (c, done)
	return P.call {
		proto = 'https',
		blowfish = false,

		params = P.params_user(c, { method = 'auth.partnerLogin' }),
		data = {
			deviceModel = 'android-generic',
			username = 'android',
			password = 'AC7IBG09A3DTSYM4R41UJWL07VLN8JI7',
			version = '5',
		},
		done = function (r, st)
			if st.stat ~= 'ok' then
				done(nil, 'server_error')
				return
			end

			r.syncTime = tostr(r.syncTime)
			r.partnerId = tostr(r.partnerId)
			r.partnerAuthToken = tostr(r.partnerAuthToken)

			done {
				partner_time_offset = tonumber(P.decode_synctime(r.syncTime)) - os.time(),
				partner_id = r.partnerId,
				partner_auth_token = r.partnerAuthToken,
			}
		end,
	}
end

P.user_login = function (c, done)
	local v = false

	return P.call {
		proto = 'https',
		blowfish = true,

		params = { 
			method = 'auth.userLogin',
			partner_id = c.partner_id,
			auth_token = c.partner_auth_token,
		},
		data = P.data_user(c, {
			loginType = 'user',
			username = c.username,
			password = c.password,
			syncTime = os.time() + c.partner_time_offset,
			partnerAuthToken = c.partner_auth_token,
		}),

		done = function (r, st)
			if st.stat ~= 'ok' then
				if v then info(st) end
				done(nil, 'invalid_userpass')
				return
			end

			done {
				user_id = tostr(r.userId),
				user_auth_token = tostr(r.userAuthToken),
			}
		end,
	}
end

P.handle_common = function (r, st, done)
	if st.code == 1001 or st.code == 1010 or st.code == 13 then
		done(nil, 'invalid_token')
	elseif not r then
		done(nil, 'server_error')
	else
		done(r)
	end
end

P.grep_songs = function (old, station_id)
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
			station_id = station_id,
			cover_url = s.albumArtUrl,
			like = (s.songRating == 1),
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

P.choose_genres = function (c, done)
	return P.call {
		proto = 'http',
		blowfish = true,
		params = P.params_user(c, {
			method = 'station.createStation',
		}),
		data = P.data_user(c, {
			musicToken = c.genres_id,
		}),
		done = function (r, st) 
			P.handle_common(r, st, done)
		end,
	}
end

P.stations_list = function (c, done)
	return P.call {
		proto = 'http',
		blowfish = true,
		params = P.params_user(c, {
			method = 'user.getStationList',
		}),
		data = P.data_user(c, {}),
		done = function (r, st) 
			if r then r = P.grep_stations(r) end
			P.handle_common(r, st, done)
		end,
	}
end

P.genres_list = function (c, done)
	return P.call {
		proto = 'http',
		blowfish = true,
		params = P.params_user(c, {
			method = 'station.getGenreStations',
		}),
		data = P.data_user(c, {}),
		done = function (r, st) 
			if r then r = P.grep_genres(r) end
			P.handle_common(r, st, done)
		end,
	}
end

P.songs_list = function (c, done)
	return P.call {
		proto = 'https',
		blowfish = true,
		params = P.params_user(c, {
			method = 'station.getPlaylist',
		}),
		data = P.data_user(c, {
			stationToken = c.station_id,
		}),
		done = function (r, st)
			if r then r = P.grep_songs(r, c.station_id) end
			P.handle_common(r, st, done)
		end,
	}
end

P.add_feedback = function (c, done)
	return P.call {
		proto = 'http',
		blowfish = true,
		params = P.params_user(c, {
			method = 'station.addFeedback',
		}),
		data = P.data_user(c, {
			trackToken = c.song_id,
			isPositive = c.like,
		}),
		done = function (r, st)
			P.handle_common(r, st, done)
		end,
	}
end

P.login = function (c, done)
	local v = false
	local ret = {}

	P.partner_login({}, function (r, err)
		if err then
			done(r, err)
			return
		end

		ret.partner_id = r.partner_id
		ret.partner_time_offset = r.partner_time_offset
		ret.partner_auth_token = r.partner_auth_token
		ret.username = c.username
		ret.password = c.password
		if v then info(ret) end

		P.user_login(ret, function (r, err)
			if v then info(r, err) end
			if r then
				ret.user_auth_token = r.user_auth_token
				ret.user_id = r.user_id
				done(ret)
			else
				done(nil, err)
			end
		end)
	end)
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

P.stations_list = function (c, done)
	return P.call {
		proto = 'http',
		blowfish = true,
		params = P.params_user(c, {
			method = 'user.getStationList',
		}),
		data = P.data_user(c, {}),
		done = function (r, st) 
			if r then r = P.grep_stations(r) end
			P.handle_common(r, st, done)
		end,
	}
end

P.auto_auth = function (c, call, done, fail, log)
	log = log or function () end

	local choose_default_station = function ()
		log('getting default station')
		P.stations_list(c, function (r, err)
			if err then
				fail(err)
			end
			c.station_id = r[1].id

			log('fetching songs')
			call(c, function (r, err)
				if err then fail(err) else done(r) end
			end)
		end)
	end

	local login = function ()
		log('login')
		P.login(c, function (r, err)
			if err then
				fail(err)
				return
			end

			table.add(c, r)
			choose_default_station()
		end)
	end
	
	if not c.username or not c.password then
		set_immediate(function () fail('need_auth') end)
		return
	end

	if not P.has_all_user_cookie(c) then
		login()
		return
	end

	log('fetching songs')
	call(c, function (r, err)
		if not err then
			done(r)
		elseif err == 'invalid_token' then
			login()
		else
			fail(err)
		end
	end)
end

P.setopt_genres_choose = function (o, done, fail)
	local task = P.restart_and_fetch_songs()

	local c = table.copy(P.cookie)
	c.station_id = nil
	c.genres_id = o.id

	task.on_fail = function (err)
		done{result=1, msg=err}
	end

	task.on_done = function (r, c)
		P.setcookie(c)
		done{result=0}
	end

	task.on_cancelled = function ()
		done{result=1, msg='cancelled'}
	end

	P.auto_auth(c, P.choose_genres, function (r)
		c.station_id = r.stationId
		P.auto_auth(c, P.songs_list, function (r)
			task.done(r, c)
		end, task.fail)
	end, task.fail)
end

P.setopt_station_choose = function (o, done)
	local task = P.restart_and_fetch_songs()

	local c = table.copy(P.cookie)
	c.station_id = o.id

	task.on_fail = function (err)
		done{result=1, msg=err}
	end

	task.on_done = function (r, c)
		P.setcookie(c)
		done{result=0}
	end

	P.auto_auth(c, P.songs_list, function (r)
		task.done(r, c)
	end, task.fail)
end

P.setopt_login = function (o, done)
	local task = P.restart_and_fetch_songs()

	local c = table.copy(P.cookie)
	P.clean_user_cookie(c)
	c.username = o.username
	c.password = o.password

	task.on_done = function (r, c)
		P.setcookie(c)
		done{result=0}
	end

	task.on_fail = function (err)
		done{result=1, msg=err}
	end

	P.auto_auth(c, P.songs_list, function (r)
		task.done(r, c)
	end, task.fail, task.log)
end

P.setopt_genres_list = function (o, done)
	local c = table.copy(P.cookie)

	P.auto_auth(c, P.genres_list, function (r)
		done{result=0, genres=r}
	end, function (err)
		done{result=1, msg=err}
	end)
end

P.setopt_stations_list = function (o, done)
	local c = table.copy(P.cookie)

	P.auto_auth(c, P.stations_list, function (r)
		done{result=0, stations=r}
	end, function (err)
		done{result=1, msg=err}
	end)
end

P.setopt_rate = function (o, done)
	local s = P.cursong() or {}

	o.id = o.id or s.id
	local like = (o.op == 'pandora.rate_like')
	if s then s.like = like end

	local c = table.add({song_id=o.id, like=like}, P.cookie)
	P.add_feedback(c, function ()
		done{result=0}
	end)

	if not like then
		P.skip()
	end
end

P.setopt = function (o, done)
	done = done or function () end
	if o.op == 'pandora.genre_choose' and isstr(o.id) then
		P.setopt_genres_choose(o, done)
		return true
	elseif o.op == 'pandora.station_choose' and isstr(o.id) then
		P.setopt_station_choose(o, done)
		return true
	elseif o.op == 'pandora.login' and isstr(o.username) and isstr(o.password) then
		P.setopt_login(o, done)
		return true
	elseif o.op == 'pandora.genres_list' then
		P.setopt_genres_list(o, done)
		return true
	elseif o.op == 'pandora.stations_list' then
		P.setopt_stations_list(o, done)
		return true
	elseif o.op == 'pandora.rate_like' or o.op == 'pandora.rate_ban' then
		P.setopt_rate(o, done)
		return true
	end
end

P.info_login = function ()
	return {login=true, username=D.cookie.username, password=D.cookie.password}
end

P.name = 'pandora'

P.prefetch_songs = function (task)
	local c = table.copy(P.cookie)

	task.on_done = function (r, c)
		P.setcookie(c)
	end

	P.auto_auth(c, P.songs_list, function (r)
		task.done(r, c)
	end, task.fail, task.log)
end

P.init = function ()
	P.cookie = P.loadcookie()
end

P.init()

pandora = playlist.station(P)

