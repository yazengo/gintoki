
playlist = function (popt)

local P = {}

P.mode = 'repeat_all'

P.loadlist = function (dir) 
	local list = {}
	local files = os.readdir(dir)
	for i,fname in ipairs(files) do
		list[i] = {
			url = dir..'/'..fname, 
			title = os.basename(fname),
			id = tostring(i),
			cover_url = '',
		}
	end
	return list
end

P.loadlist_fromdirs = function (dirs) 
	for i,dir in ipairs(dirs) do
		local l = P.loadlist(dir)
		if table.maxn(l) > 0 then
			return l
		end
	end
	return {}
end

P.log_list = {}
P.log_max = 40
P.log_i = 1

P.list = P.loadlist_fromdirs(popt.dirs)
P.i = 1

P.addlist = function (newfile)
    for _, file in ipairs(P.list) do
        if file.url == newfile then return end
    end
    index = table.maxn(P.list)
    table.insert(P.list, {
        url = newfile,
        title = os.basename(newfile),
        id = index + 1,
        cover_url = '',
    })
end

P.setopt = function (o, done)
	done = done or function () end

	if o.op == 'audio.prev' then
		P.do_prev()
		done{result=0}
		return true
	end

	if o.op == 'audio.play' and o.id then
		local i = tonumber(o.id)
		if i > 0 and i <= table.maxn(P.list) then
			P.i = i-1
			P.skip()
		end

		done{result=0}
		return true
	end

	if o.op == 'audio.toggle_repeat_mode' then
		if P.mode == 'repeat_all' then
			P.mode = 'repeat_one'
		elseif P.mode == 'repeat_one' then
			P.mode = 'repeat_all'
		end

		done{result=0, mode=P.mode}
		return true
	end

	if o.op == popt.type .. '.set_play_mode' then
		if o.mode == 'repeat_all' then
			P.mode = o.mode
		elseif o.mode == 'repeat_one' then
			P.mode = o.mode
		elseif o.mode == 'normal' then
			P.mode = o.mode
		elseif o.mode == 'shuffle' then
			P.mode = o.mode
		end

		done{result=0, mode=P.mode}
		return true
	end

	if o.op == popt.type .. '.songs_list' then
		done{result=0, songs_list=P.list}
		return true
	end
end

P.skip = function ()
	if P.on_skip then P.on_skip() end
end

P.stop = function () end

P.next = function (o, done)
	if table.maxn(P.list) == 0 then return end

	if P.next_song then
		done(P.next_song)
		P.next_song = nil
		return
	end

	if P.mode == 'repeat_one' and o and o.normal_end then
		done(P.list[P.i])
	end

	if P.mode == 'repeat_all' then
		P.i = P.i+1
		if P.i > table.maxn(P.list) then
			P.i = 1
		end
		done(P.list[P.i])
	elseif P.mode == 'normal' or P.mode == 'repeat_one' then
		P.i = P.i+1
		if P.i > table.maxn(P.list) then
			return 
		end
		done(P.list[P.i])
	elseif P.mode == 'shuffle' then
		P.i = math.random(1, table.maxn(P.list))

		P.log_list[P.log_i] = P.list[P.i]
		P.log_i = P.log_i+1
		if table.maxn(P.log_list) > P.log_max then
			P.log_list[P.log_i-P.log_max] = nil
		end

		done(P.list[P.i])
	end
end

P.do_prev = function ()
	if table.maxn(P.list) == 0 then return end

	if P.mode == 'repeat_all' then
		P.i = P.i-1
		if P.i <= 0 then
			P.i = table.maxn(P.list)
		end
		P.next_song = P.list[P.i]
	elseif P.mode == 'normal' or P.mode == 'repeat_one' then
		P.i = P.i-1
		if P.i <= 0 then
			return
		end
		P.next_song = P.list[P.i]
	elseif P.mode == 'shuffle' then
		if not P.log_list[P.log_i] then
			P.i = math.random(1, table.maxn(P.list))
			P.next_song = P.list[P.i]
		else
			P.log_i = P.log_i-1
			P.next_song = P.log_list[P.log_i]
		end
	end

	if P.on_skip then P.on_skip() end
end

P.info = function ()
	return {type=popt.type, play_mode=P.mode}
end

return P
end

