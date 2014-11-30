
require('prop')
require('radio')

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
}

>> access_token invalid
<< {
	"code":106,
	"msg":"access_token_has_expired : 949b9d5e96c99e38bdd2d81bc45cdaa8"
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
	return prop.get('douban.cookie', {})
end

D.savecookie = function (c)
	info('cookie saved', c)
	prop.set('douban.cookie', c)
end

D.setcookie = function (c)
	D.cookie = c
	D.savecookie(c)
end

D.curl = function (p)
	if p.access_token then
		p.headers = {
			Authorization = 'Bearer ' .. p.access_token,
		}
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

--
-- API
--
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
		done = function (rs, st)
			local r = cjson.decode(rs) or {}
			if r.access_token then
				info('login', c.username, c.password, 'ok')
				D.user_info({access_token=r.access_token}, done)
			elseif r.code == 120 then
				info('login', c.username, c.password, 'error', rs)
				done(nil, 'invalid_userpass')
			else
				info('login', c.username, c.password, 'error', rs)
				done(nil, 'server_error')
			end
		end,
	}
end

D.channels_list = function (c, done)
	local p = {
		url = 'https://api.douban.com/v2/fm/app_channels?' .. encode_params(D.common_params()),
		access_token = c.access_token,
		done = function (rs, st)
			local r = cjson.decode(rs) or {}
			if r.groups then
				done(r, nil)
			elseif r.msg then
				info('channels_list', 'error', rs)
				done(nil, 'invalid_token')
			else
				info('channels_list', 'error', rs)
				done(nil, 'server_error')
			end
		end,
	}
	return D.curl(p)
end

D.songs_list = function (c, done)
	if c.channel == nil then
		c.channel = 0
	end
	return D.curl {
		access_token = c.access_token,
		url = 'https://api.douban.com/v2/fm/playlist?' .. encode_params(D.common_params{
			type = 'n',
			channel = c.channel,
		}),
		done = function (r, st)
			r = cjson.decode(r) or {}
			s = D.grep_songs(r.song or {}, c.channel)
			if table.maxn(s) > 0 then
				done(s, nil)
			elseif r.msg then
				done(nil, 'invalid_token')
			elseif r.r == 1 then
				done(nil, 'wrong_channel')
			else
				done(nil, 'server_error')
			end
		end,
	}
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

D.grep_songs = function (songs, channel_id)
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
			channel_id = channel_id,
		})
	end
	return r
end

D.rate = function (c, type, sid, done)
	local s = D.cursong()
	if not sid then sid = s.id end
	if type == 't' then
		if s and s.like then type = 'u' else type = 'r' end
	end
	if s and type == 'r' then s.like = true end
	if s and (type == 'u' or type == 'b') then s.like = false end
	D.report(table.add({id=sid, type=type}, D.cookie), done)
end

D.auto_auth = function (c, call, done, fail, log) 
	log = log or function () end

	local do_call = function ()
		call(c, function (r, err)
			if err then fail(err) else done(c, r) end
		end)
	end

	local login = function ()
		log('login')
		D.user_login(c, function (r, err)
			if not err then
				c.name = r.name
				c.icon = r.icon
				c.access_token = r.access_token
				do_call()
			elseif err == 'invalid_userpass' then
				c.name = nil
				c.icon = nil
				c.username = nil
				c.password = nil
				c.access_token = nil
				do_call()
			else
				fail(err)
			end
		end)
	end

	local change_default_channel = function ()
		log('fetching songs in default channel')
		c.channel = 0
		do_call()
	end

	if c.username and not c.access_token then
		login()
	else
		log('fetching songs')
		call(c, function (r, err)
			if not err then
				done(nil, r)
			elseif err == 'invalid_token' then
				login()
			elseif err == 'wrong_channel' then
				change_default_channel()
			else
				fail(err)
			end
		end)
	end
end

D.prefetch_songs = function (task)
	task.on_done = function (r, c)
		if c then D.setcookie(c) end
	end

	D.auto_auth(D.cookie, D.songs_list, function (c, r)
		task.done(r, c)
	end, task.fail, task.log)
end

D.setopt_login = function (o, done)
	local task = D.restart_and_fetch_songs()

	local c = table.copy(D.cookie)
	c.username = o.username
	c.password = o.password
	c.access_token = nil
	c.name = nil
	c.icon = nil

	task.on_done = function (r, c)
		if c then D.setcookie(c) end
		done{result=0, icon=c.icon, name=c.name}
	end

	task.on_fail = function (err)
		done{result=1, msg=err}
	end

	task.on_cancelled = function ()
		done{result=1, msg='cancelled'}
	end

	D.auto_auth(c, D.songs_list, function (c, r)
		if not c or not c.username then
			task.fail('invalid_password')
			return
		end
		task.done(r, c)
	end, task.fail)
end

D.setopt_channel_choose = function (o, done) 
	local task = D.restart_and_fetch_songs()

	local c = table.copy(D.cookie)
	c.channel = o.id

	task.on_done = function (r, c)
		if c then D.setcookie(c) end
		done{result=0}
	end

	task.on_fail = function (err)
		done{result=1, msg=err}
	end

	task.on_cancelled = function ()
		done{result=1, msg='cancelled'}
	end

	D.auto_auth(c, D.songs_list, function (c, r)
		task.done(r, c)
	end, task.fail)
end

D.setopt_channels_list = function (o, done) 
	local c = table.copy(D.cookie)

	D.auto_auth(c, D.channels_list, function (c, r) 
		if c then D.setcookie(c) end
		r.result = 0
		done(r)
	end, function (err)
		done{result=1, msg=err}
	end)
end

D.setopt_logout = function (o, done)
	local task = D.restart_and_fetch_songs()

	local c = D.cookie
	c.username = nil
	c.password = nil
	c.access_token = nil
	c.name = nil
	c.icon = nil

	task.on_done = function (r, c)
		D.setcookie(c)
		done{result=0}
	end

	task.on_fail = function (err)
		done{result=1, msg=err}
	end

	task.on_cancelled = function ()
		done{result=1, msg='cancelled'}
	end

	D.auto_auth(c, D.songs_list, function (c, r)
		task.done(r, c)
	end, task.fail)
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
	elseif o.op == 'douban.rate_ban' then
		D.rate(D.cookie, 'b', o.id, function () 
			done{result=0} 
			D.skip()
		end)
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

D.name = 'douban'

D.init = function ()
	D.cookie = D.loadcookie()
end

D.init()

douban = radio.new_station(D)

