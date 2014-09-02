
local I = {}

I.cmds = {
	[[ audio.pause_resume_toggle() ]],
	[[ radio.next() ]],
	[[ audio.setvol(audio.getvol() - 10); print(audio.getvol()) ]],
	[[ audio.setvol(audio.getvol() + 10); print(audio.getvol()) ]],
}

I.handle = function (line) 
	if line == '' then
		I.usage()
		return
	end
	local i = tonumber(line)
	if i and i >= 1 and i <= table.maxn(I.cmds) then
		local cmd = I.cmds[i]
		print('dostring: ' .. cmd)
		loadstring(cmd)()
	end
end

I.usage = function ()
	print('--- select command ---')
	for k,v in pairs(I.cmds) do
		print(k .. ' ' .. v)
	end
end

input = I

stdin_open(input.handle)

