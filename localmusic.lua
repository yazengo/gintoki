
local P

P = {}

P.mode = 'repeat_all'

P.loadlist = function (dir) 
	info('loading musics from', dir)
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
	info('total', table.maxn(list), 'songs')
	return list
end

P.log_list = {}
P.log_max = 40
P.log_i = 1

P.list = P.loadlist(os.getenv('MUSIC_DIR') or '/mnt/sdcard/musics')
if table.maxn(P.list) == 0 then
	P.list = P.loadlist('musics')
end
P.i = 1

P.setopt = function (opt, done)
	done = done or function () end

	if opt.op == 'audio.play' and opt.id then
		local i = tonumber(opt.id)
		if i > 0 and i <= table.maxn(P.list) then
			P.i = i-1
			if P.next_callback then P.next_callback() end
		end

		done{result=0}
		return true
	end

	if opt.op == 'local.toggle_repeat_mode' then
		if P.mode == 'repeat_all' then
			P.mode = 'repeat_one'
		elseif P.mode == 'repeat_one' then
			P.mode = 'repeat_all'
		end

		done{result=0, mode=P.mode}
		return true
	end

	if opt.op == 'local.set_play_mode' then
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

	if opt.op == 'local.songs_list' then
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
	return {type='local', play_mode=P.mode}
end

localmusic = P

