
local P

P = {}

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

P.log_list = {}
P.log_max = 40
P.log_i = 0

P.list = P.loadlist('musics')
P.i = 0

P.setopt = function (opt)
	if opt.id then
		local i = tonumber(opt.id)
		if i >= 0 and i < table.maxn(P.list) then
			P.i = i
			if P.next_callback then P.next_callback() end
		end
	end

	if opt.mode == 'repeat_all' then
		P.mode = opt.mode
	elseif opt.mode == 'repeat_one' then
		P.mode = opt.mode
	elseif opt.mode == 'normal' then
		P.mode = opt.mode
	elseif opt.mode == 'shuffle' then
		P.mode = opt.mode
	end

end

-- return song or nil
P.next = function ()
	if P.mode == 'repeat_all' then
		P.i = P.i+1
		if P.i == table.maxn(P.list) then
			P.i = 0
		end
		return P.list[P.i]
	elseif P.mode == 'repeat_one' then
		return P.list[P.i]
	elseif P.mode == 'normal' then
		P.i = P.i+1
		if P.i == table.maxn(P.list) then
			return nil
		end
		return P.list[P.i]
	elseif P.mode == 'shuffle' then
		P.i = math.random(0, table.maxn(P.list)-1)

		P.log_list[P.log_i+1] = P.list[P.i]
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
		if P.i < 0 then
			P.i = 0
		end
		return P.list[P.i]
	elseif P.mode == 'repeat_one' then
		return P.list[P.i]
	elseif P.mode == 'normal' then
		P.i = P.i-1
		if P.i < 0 then
			return nil
		end
		return P.list[P.i]
	elseif P.mode == 'shuffle' then
		if not P.log_list[P.log_i] then
			P.i = math.random(0, table.maxn(P.list)-1)
			return P.list[P.i]
		else
			P.log_i = P.log_i-1
			return P.log_list[P.log_i+1]
		end
	end
end

P.info = function ()
	return {type='local', mode=P.mode}
end

localmusic = P

