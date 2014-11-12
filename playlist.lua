
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
	if table.maxn(list) > 0 then 
		info(dir, table.maxn(list), 'songs')
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

P.setopt = function (opt, done)
	done = done or function () end

	if opt.op == popt.type .. '.play' and opt.id then
		local i = tonumber(opt.id)
		if i > 0 and i <= table.maxn(P.list) then
			P.i = i-1
			if P.next_callback then P.next_callback() end
		end

		done{result=0}
		return true
	end

	if opt.op == popt.type .. '.toggle_repeat_mode' then
		if P.mode == 'repeat_all' then
			P.mode = 'repeat_one'
		elseif P.mode == 'repeat_one' then
			P.mode = 'repeat_all'
		end

		done{result=0, mode=P.mode}
		return true
	end

	if opt.op == popt.type .. '.set_play_mode' then
		if opt.mode == 'repeat_all' then
			P.mode = opt.mode
		elseif opt.mode == 'repeat_one' then
			P.mode = opt.mode
		elseif opt.mode == 'normal' then
			P.mode = opt.mode
		elseif opt.mode == 'shuffle' then
			P.mode = opt.mode
		end

		done{result=0, mode=P.mode}
		return true
	end

	if opt.op == popt.type .. '.songs_list' then
		done{result=0, songs_list=P.list}
		return true
	end
end

-- return song or nil
P.next = function (opt)
	if P.mode == 'repeat_one' and opt and opt.playdone then
		return P.list[P.i]
	end

	if P.mode == 'repeat_all' then
		P.i = P.i+1
		if P.i > table.maxn(P.list) then
			P.i = 1
		end
		return P.list[P.i]
	elseif P.mode == 'normal' or P.mode == 'repeat_one' then
		P.i = P.i+1
		if P.i > table.maxn(P.list) then
			return nil
		end
		return P.list[P.i]
	elseif P.mode == 'shuffle' then
		P.i = math.random(1, table.maxn(P.list))

		P.log_list[P.log_i] = P.list[P.i]
		P.log_i = P.log_i+1
		if table.maxn(P.log_list) > P.log_max then
			P.log_list[P.log_i-P.log_max] = nil
		end

		return P.list[P.i]
	end
end

P.prev = function ()
	if P.mode == 'repeat_all' then
		P.i = P.i-1
		if P.i <= 0 then
			P.i = table.maxn(P.list)
		end
		return P.list[P.i]
	elseif P.mode == 'normal' or P.mode == 'repeat_one' then
		P.i = P.i-1
		if P.i <= 0 then
			return nil
		end
		return P.list[P.i]
	elseif P.mode == 'shuffle' then
		if not P.log_list[P.log_i] then
			P.i = math.random(1, table.maxn(P.list))
			return P.list[P.i]
		else
			P.log_i = P.log_i-1
			return P.log_list[P.log_i]
		end
	end
end

P.info = function ()
	return {type=popt.type, play_mode=P.mode}
end

return P
end

