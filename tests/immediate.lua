
i = 0
loop = function ()
	i = i + 1
	set_immediate(function ()
		info('imm', i)
		if i > 1000 then
			return
		end
		loop()
	end)
end

loop()

