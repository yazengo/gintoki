
local I = {}

I.cmds = {
	[[ audio.pause_resume_toggle() ]],
	[[ radio.next() ]],
	[[ audio.setvol(audio.getvol() - 10); print(audio.getvol()) ]],
	[[ audio.setvol(audio.getvol() + 10); print(audio.getvol()) ]],
	[[ radio.change{type = 'pandora'} ]],
	[[ radio.change{type = 'local'} ]],
	[[ gsensor_next() ]],
	[[ gsensor_prev() ]],
}

I.handle = function (line) 
	if line == '' then
		I.usage()
		return
	end

	argv = string.split(line)

	local i = tonumber(argv[1])

	if i and i >= 1 and i <= table.maxn(I.cmds) then
		local cmd = I.cmds[i]
		print('dostring: ' .. cmd)
		local func = loadstring(cmd)
		local r, err = pcall(func)
		if err then
			print(err)
		end
	end

	if argv[1] == 'c' then
		local f, err = loadstring(string.sub(line, 2))
		if err then
			print(err)
			return
		end
		local r, err = pcall(f)
		if not r then
			print(err)
		end
	end

end

I.usage = function ()
	print('--- select command ---')
	for k,v in pairs(I.cmds) do
		print(k .. ' ' .. v)
	end
	print('c  [command]')
end

input = I

stdin_open(input.handle)

