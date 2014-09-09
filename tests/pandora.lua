
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

local P = {}

P.url = '://tuner.pandora.com/services/json/?'

P.log = logger('pandora')

P.cookies = {}

P.bf_encode = blowfish('6#26FRL$ZWD')
P.bf_decode = blowfish('R=U!LH$O2B#')

P.loadcookie = function ()
	return loadjson('pandora.json')
end

P.savecookie = function (c)
	P.log('cookie saved', c)
	savejson('pandora.json', c)
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

	if P.verbose then log(url, p.data) end

	return curl {
		proxy = 'localhost:8888',
		url = url,
		content_type = 'text/plain',
		user_agent = 'pithos',
		reqstr = reqstr,
		done = function (r, stat) 
			if P.verbose then log(r, stat) end
			if stat.stat == 'cancelled' then
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
				return
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

P.choose_genres = function (cookie, done)
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
			musicToken = cookie.genres_id,
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

--
-- err = 'auth_failed/need_station_id/server_error'
--
P.auto_auth = function (cookie, cb, done)
	local log = logger('auto_auth:')
	local handle

	handle = {
		cancel = function ()
			if handle.task then
				log('cancelled')
				handle.task.cancel()
				handle.task = nil
			end
		end
	}

	function funcname(f) 
		for k,v in pairs(P) do
			if v == f then
				return k
			end
		end
	end

	function on_done(func, r, stat)
		if r then on_ok(func, r, stat) else on_error(func, r, stat) end
	end

	function on_ok(func, r, stat)
		log(funcname(func), '-> ok', r)

		if func == P.partner_login then
			table.add(cookie, r)
			handle.task = P.user_login(cookie, function (...) on_done(P.user_login, ...) end)
		elseif func == P.user_login then
			table.add(cookie, r)
			handle.task = cb(cookie, function (...) on_done(cb, ...) end)
		else
			done(cookie, r, nil)
		end
	end

	function on_error(func, r, stat)
		log(funcname(func), '-> error')

		if func == P.partner_login then
			done(cookie, nil, 'auth_failed')
		elseif func == P.user_login then
			done(cookie, nil, 'auth_failed')
		else 
			if stat.code == 1001 or stat.code == 1010 then
				handle.task = P.partner_login(cookie, function (...) on_done(P.partner_login, ...) end)
			elseif func == P.choose_genres or func == P.songs_list then
				done(cookie, nil, 'need_station_id')
			else
				done(cookie, nil, 'server_error')
			end
		end
	end

	if not cookie.partner_id or not cookie.partner_time_offset or not cookie.partner_auth_token or
		not cookie.user_id or not cookie.user_auth_token
	then
		handle.task = P.partner_login(cookie, function (...) on_done(P.partner_login, ...) end)
	elseif func == P.songs_list and not cookie.station_id then
		done(cookie, nil, 'need_station_id')
	else
		handle.task = cb(cookie, function (...) on_done(cb, ...) end)
	end

	return handle
end

P.start = function ()
	P.stat = 'songs_ready'
	P.next()
end

P.setopt = function (o, done)
	done = done or function () end

	P.log('setopt', o)

	if o.op == 'pandora.genre_choose' then

		if not o.id then
			P.log('need id')
			return
		end

		P.stat = 'songs_fetching'
		if P.task then P.task.cancel() end
		P.songs = {}
		P.songs_i = 0

		local c = P.cookie
		c.station_id = nil
		c.genres_id = o.id

		P.task = P.auto_auth(c, P.choose_genres, function (c, r, err)
			if not r or not r.stationId then
				done{result=1}
				P.stat = 'need_station_id'
				return
			end

			c.station_id = r.stationId

			P.task = P.auto_auth(c, P.songs_list, function (c, r, err)
				P.savecookie(c)
				if r then
					P.stat = 'songs_ready'
					P.songs_add(r)
					done{result=0}
				else
					P.stat = err
					done{result=1, err=err}
				end
			end)
		end)

	elseif o.op == 'pandora.station_choose' then

		if not o.id then
			P.log('need id')
			return
		end

		P.stat = 'songs_fetching'
		if P.task then P.task.cancel() end
		P.songs = {}
		P.songs_i = 0

		local c = P.cookie
		c.station_id = o.id

		P.task = P.auto_auth(c, P.songs_list, function (c, r, err)
			P.savecookie(c)
			if r then
				P.stat = 'songs_ready'
				P.songs_add(r)
				done{result=0}
			else
				P.stat = err
				done{result=1, err=err}
			end
		end)

	elseif o.op == 'pandora.login' then

		P.stat = 'auth_processing'
		if P.task then P.task.cancel() end
		P.songs = {}
		P.songs_i = 0

		local c = P.cookie
		c.user_auth_token = nil
		c.user_id = nil
		c.partner_auth_token = nil
		c.partner_time_offset = nil
		c.partner_id = nil
		c.username = o.username
		c.password = o.password

		P.task = P.auto_auth(c, P.stations_list, function (c, r, err)
			if not r then
				P.stat = err
				done{err=1}
				return
			end

			if table.maxn(r) == 0 then
				P.stat = 'need_station_id'
				done{err=0}
				return
			end
			c.station_id = r[1].id

			P.task = P.auto_auth(c, P.songs_list, function (c, r, err)
				P.savecookie(c)
				if r then
					P.stat = 'songs_ready'
					P.songs_add(r)
					done{err=0}
				else
					P.stat = err
					done{err=1}
				end
			end)
		end)

	elseif o.op == 'pandora.genres_list' then
		local c = table.copy(P.cookie)
		P.auto_auth(c, P.genres_list, function (c, r, err)
			if r then
				done(r)
			else
				done{result=1}
			end
		end)
	elseif o.op == 'pandora.stations_list' then
		local c = table.copy(P.cookie)
		P.auto_auth(c, P.stations_list, function (c, r, err)
			if r then
				done(r)
			else
				done{result=1}
			end
		end)
	elseif o.op == 'pandora.songs_list' then
		P.songs_list(P.cookie, function () end)
	end
end

--[[
need_auth
need_station_id
auth_processing
auth_failed
songs_fetching
songs_ready
]]--

P.songs = {}
P.songs_i = 0

P.songs_add = function (r)
	P.log('got songs nr', table.maxn(r))
	local left = table.maxn(P.songs) - P.songs_i
	table.append(P.songs, r)
	if left == 0 and P.next_callback then
		P.next_callback()
	end
end

P.next = function ()
	local left = table.maxn(P.songs) - P.songs_i

	if left <= 1 and P.stat == 'songs_ready' then
		P.stat = 'songs_fetching'
		P.task = P.auto_auth(P.cookie, P.songs_list, function (c, r, err)
			P.savecookie(c)
			P.stat = 'songs_ready'
			if r then
				P.songs_add(r)
			else
				P.stat = err
			end
		end)
	end

	if left == 0 then
		return nil
	end

	P.songs_i = P.songs_i + 1
	return P.songs[P.songs_i]
end

P.info = function ()
	return {
		type = 'pandora',
		songs_nr = table.maxn(P.songs),
		songs_i = P.songs_i,
		stat = P.stat,
	}
end

pandora = P

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

