
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
		}
	end
	return list
end

P.list = P.loadlist('musics')
P.i = 0

P.setopt = function (opt)
	local i = tonumber(opt.id)
	if i >= 0 and i < table.maxn(P.list) then
		P.i = i
		if P.next_callback then P.next_callback() end
	end
end

-- return song or nil
P.next = function ()
	local s = P.cursong()
	P.i = P.i + 1
	return s
end

P.prev = function ()
	local s = P.cursong()
	P.i = P.i - 1
	return s
end

P.cursong = function () 
	return P.list[(P.i%table.maxn(P.list))+1]
end

P.info = function ()
	return {type='local', mode=P.mode}
end

localmusic = P

