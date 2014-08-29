
local P = {}

P.log = function (...) 
	info('pandora:', ...)
end

P.info = function ()
	return {type='pandora'}
end

P.songs = {}
P.songs_i = 0

P.callapi = function (opt, done)
	return curl {
		url = 'sugrsugr.com:8083',
		body = cjson.encode(opt),
		done = function (ret)
			local js = cjson.decode(ret) or {}
			done(js)
		end,
	}
end

P.fetch_done = function (songs) 
	local space = table.maxn(P.songs) - P.songs_i
	table.append(P.songs, songs)
	if space == 0 then
		if P.next_callback then P.next_callback() end
	end
end

P.fetch = function ()
	if P.fetch_task then return end

	local done = function (js)
		P.fetch_task = nil
		local songs = js.songs or {}
		P.log('fetchdone songnr', table.maxn(songs))
		if table.maxn(songs) == 0 then
			P.log('refetch after 1000ms')
			set_timeout(P.fetch, 1000)
			return
		end
		P.fetch_done(songs)
	end

	P.log('fetching start')

	P.fetch_task = P.callapi({op='pandora.songs_list'}, done)
end

P.setopt = function (opt, setopt_done) 

	if not string.hasprefix(opt.op, 'pandora.') then
		setopt_done{}
		return
	end

	if opt.op == 'pandora.login' or 
		 opt.op == 'pandora.genre_choose' or
		 opt.op == 'pandora.station_choose'
	then

		P.songs = {}
		P.songs_i = {}
		if P.next_callback then P.next_callback() end

		P.callapi(opt, function (js)
			setopt_done(js)
			P.cancel_fetch()
			P.fetch()
		end)

	else

		P.callapi(opt, setopt_done)

	end
end

P.cancel_fetch = function ()
	if P.fetch_task then
		P.fetch_task.cancel()
		P.fetch_task = nil
	end
end

P.cursong = function ()
	local s = P.songs[P.songs_i]
	return s
end

P.next = function ()
	local space = table.maxn(P.songs) - P.songs_i

	if space < 3 then
		P.fetch()
	end

	if space == 0 then
		return nil
	end

	P.songs_i = P.songs_i + 1
	return P.cursong()
end

pandora = P

