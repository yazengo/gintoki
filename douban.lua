
--[[

songs_fetching == fetching
songs_ready == auth_ok / auth_none
server_error == fetching

>> login ok
<< access_token is set

>> login failed
<< access_token is nil

>> access_token invalid
<< {
	"code": 103, 
	"msg": "invalid_access_token: =71dfdf224d2f902e559f9d66e78eb06", 
	"request": "GET /v2/fm/playlist"
}

>> access_token ok && channel invalid
<< {
	"err": "one channel param is required", 
	"r": 1
}
<< {
	"err": "wrong channel", 
	"r": 1
}
<< {
	"is_show_quick_start": 0, 
	"r": 0, 
	"song": [], 
	"version_max": 100
}

songs_list

--]]

local D = {}

D.loadcookie = function ()
	return loadjson('douban.json')
end

D.savecookie = function (c)
	info('cookie saved', c)
	savejson('douban.json', c)
end

D.curl = function (p)
	p.retry = 1000
	if p.access_token then
		p.authorization = 'Bearer ' .. p.access_token
	end
	return curl(p)
end

D.common_params = function (p)
	return table.add({
		app_name = 'radio_sugrsugr',
		version = '89',
		client = 's:mobile|y:android|f:603|m:Douban|d:123|e:Muno',
	}, p)
end

D.user_info = function (c, done)
	return D.curl {
		access_token = c.access_token,
		url = 'https://api.douban.com/v2/fm/user_info?' .. encode_params(D.common_params()),
		done = function (r, st) 
			r = cjson.decode(r) or {}
			if r.icon and r.name then
				done({
					access_token = c.access_token,
					icon = r.icon,
					name = r.name
				})
			else
				done(nil, 'server_error')
			end
		end,
	}
end

D.user_login = function (c, done)
	return D.curl {
		url = 'https://www.douban.com/service/auth2/token',
		reqstr = encode_params {
			client_id = '02a27ea72d32847e07e77bf587978d7a',
			client_secret = 'dbac48937cdab115',
			redirect_uri = 'http://api-callback.sugrsugr.com/',
			grant_type = 'password',
			username = c.username,
			password = c.password,
		},
		done = function (r, st)
			r = cjson.decode(r) or {}
			if r.access_token then
				info('login', c.username, c.password, 'ok')
				D.user_info({access_token=r.access_token}, done)
			elseif r.code == 120 then
				info('login', c.username, c.password, 'fail')
				done(nil, 'invalid_userpass')
			else
				info('login', c.username, c.password, 'error')
				done(nil, 'server_error')
			end
		end,
	}
end

D.channels_list = function (c, done)
	local p = {
		url = 'https://api.douban.com/v2/fm/app_channels?' .. encode_params(D.common_params()),
		access_token = c.access_token,
		done = function (r, st)
			r = cjson.decode(r) or {}
			if not r.err then
				done(r, nil)
			else
				done(nil, 'server_error')
			end
		end,
	}
	return D.curl(p)
end

D.grep_songs = function (songs)
	local r = {}
	for _, _s in ipairs(songs) do
		local s = totable(_s)
		table.insert(r, {
			url = s.url,
			title = s.title,
			artist = s.artist,
			album = s.albumtitle,
			id = s.sid,
			cover_url = s.picture,
			dur = s.length,
			like = (s.like == 1),
		})
	end
	return r
end

D.songs_add = function (r)
	info('got songs nr', table.maxn(r))
	local left = table.maxn(D.songs) - D.songs_i
	table.append(D.songs, r)
	if left == -1 and D.next_callback then
		D.next_callback()
	end
end

D.report = function (c, done)
	local s; if radio and radio.song then s = radio.song end; s = s or {}
	local sid = s.id or ''
	return D.curl {
		access_token = c.access_token,
		url = 'https://api.douban.com/v2/fm/playlist?' .. encode_params(D.common_params{
			type = c.type,
			sid = s.id,
		}),
		done = function (r, st)
			done(r, nil)
		end,
	}
end

D.rate = function (c, type, sid, done)
	local s
	if radio and radio.song then s = radio.song end
	if not sid then sid = s.id end
	if type == 't' then
		if s and s.like then type = 'u' else type = 'r' end
	end
	if s and type == 'r' then s.like = true end
	if s and type == 'u' then s.like = false end
	D.report(table.add({id=sid, type=type}, D.cookie), done)
end

D.songs_list = function (c, done)
	return D.curl {
		access_token = c.access_token,
		url = 'https://api.douban.com/v2/fm/playlist?' .. encode_params(D.common_params{
			type = 'n',
			channel = c.channel,
		}),
		done = function (r, st)
			r = cjson.decode(r) or {}
			s = D.grep_songs(r.song or {})
			if table.maxn(s) > 0 then
				done(s, nil)
			elseif r.code == 103 then
				done(nil, 'invalid_token')
			elseif r.r == 1 then
				done(nil, 'wrong_channel')
			else
				done(nil, 'server_error')
			end
		end,
	}
end

D.auto_auth = function (cookie, cb, done) 
	local login = function ()
		D.user_login(cookie, function (r, err)
			if not err then
				cookie.name = r.name
				cookie.icon = r.icon
				cookie.access_token = r.access_token
				cb(cookie, function (r, err) done(cookie, r, err) end)
			elseif err == 'invalid_userpass' then
				cookie.name = nil
				cookie.icon = nil
				cookie.username = nil
				cookie.password = nil
				cookie.access_token = nil
				cb(cookie, function (r, err) done(cookie, r, err) end)
			else
				done(nil, nil, err)
			end
		end)
	end

	local change_default_channel = function ()
		cookie.channel = 0
		cb(cookie, function (r, err)
			done(cookie, r, err)
		end)
	end

	if cookie.username and not cookie.access_token then
		login()
		return
	end

	cb(cookie, function (r, err)
		if not err then
			done(nil, r, nil)
		elseif err == 'invalid_token' then
			login()
		elseif err == 'wrong_channel' then
			change_default_channel()
		else
			done(nil, nil, err)
		end
	end)
end

D.next = function (opt)
	local left = table.maxn(D.songs) - D.songs_i

	if left <= 1 and D.stat == 'songs_ready' then
		D.stat = 'songs_fetching'
		D.auto_auth(D.cookie, D.songs_list, function (c, r, err)
			if c then
				D.cookie = c
				D.savecookie(c)
			end
			D.stat = 'songs_ready'
			D.songs_add(r)
		end)
	end

	local r = D.songs[D.songs_i]
	if left > -1 then
		D.songs_i = D.songs_i + 1
	end
	return r
end

D.start = function ()
	D.cookie = D.loadcookie()
	D.songs = {}
	D.songs_i = 1
	D.stat = 'songs_ready'
	D.next()
end

D.setopt_login = function (o, done)
	D.stat = 'songs_fetching'
	D.songs = {}
	D.songs_i = 1

	local c = table.copy(D.cookie)
	c.username = o.username
	c.password = o.password
	c.access_token = nil
	c.name = nil
	c.icon = nil

	local finish = function (r)
		done(r)
		D.stat = 'songs_ready'
	end

	D.auto_auth(c, D.songs_list, function (c, r, err)
		if err then
			finish{result=1}
			return
		end

		if not c or not c.username then
			finish{result=1}
			return
		end

		if c then
			D.cookie = c
			D.savecookie(c)
		end

		D.songs_add(r)
		finish{result=0, icon=c.icon, name=c.name}
	end)
end

D.setopt_channel_choose = function (o, done) 
	D.stat = 'songs_fetching'
	D.songs = {}
	D.songs_i = 1
	if D.stop_callback then D.stop_callback() end

	local c = table.copy(D.cookie)
	c.channel = o.id

	local finish = function (r)
		done(r)
		D.stat = 'songs_ready'
	end

	D.auto_auth(c, D.songs_list, function (c, r, err)
		if err then
			finish{result=1}
			return
		end

		if c then
			D.cookie = c
			D.savecookie(c)
		end

		D.songs_add(r)
		finish{result=0}
	end)
end

D.setopt_channels_list = function (o, done) 
	D.channels_list(D.cookie, function (r) 
		if r then
			r.result = 0
			done(r)
		else
			done{result=1}
		end
	end)
end

D.setopt_logout = function (o, done)
	D.stat = 'songs_fetching'
	D.songs = {}
	D.songs_i = 1

	local c = D.cookie
	c.username = nil
	c.password = nil
	c.access_token = nil
	c.name = nil
	c.icon = nil
	D.savecookie(c)

	if D.stop_callback then D.stop_callback() end

	local finish = function (r)
		done(r)
		D.stat = 'songs_ready'
	end

	D.auto_auth(c, D.songs_list, function (c, r, err)
		if err then
			finish{result=1}
			return
		end

		D.songs_add(r)
		finish{result=0}
	end)
end

D.setopt = function (o, done)
	done = done or function (r) end

	if o.op == 'douban.login' and isstr(o.username) and isstr(o.password) then
		D.setopt_login(o, done)
		return true
	elseif o.op == 'douban.logout' then
		D.setopt_logout(o, done)
		return true
	elseif o.op == 'douban.channel_choose' and o.id then
		D.setopt_channel_choose(o, done)
		return true
	elseif o.op == 'douban.channels_list' then
		D.setopt_channels_list(o, done)
		return true
	elseif o.op == 'douban.stat' then
		done(table.add({result=0}, D.info_login()))
		return true
	elseif o.op == 'douban.rate_like' then
		D.rate(D.cookie, 'r', o.id, function () done{result=0} end)
		return true
	elseif o.op == 'douban.rate_unlike' then
		D.rate(D.cookie, 'u', o.id, function () done{result=0} end)
		return true
	elseif o.op == 'douban.rate_toggle_like' then
		D.rate(D.cookie, 't', o.id, function () done{result=0} end)
		return true
	end
end

D.info_login = function ()
	if D.cookie.name then
		return {login=true, name=D.cookie.name, icon=D.cookie.icon}
	else
		return {login=false}
	end
end

D.is_fetching = function ()
	return D.stat == 'songs_fetching' and table.maxn(D.songs)+1 == D.songs_i 
		or D.stat == 'server_error'
end

D.info = function ()
	return {
		type = 'douban',
		fetching = D.is_fetching(),
	}
end

douban = D

