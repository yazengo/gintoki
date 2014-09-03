
setloglevel(0)

local c = curl {
	url = 'sugrsugr.com/a.img',
	done = function (ret, stat)
		info(ret, stat)
	end,
}

i = 0
poll = function ()
	info(c.stat())
	i = i + 1
	if i == 5 then
		c.cancel()
	end
	set_timeout(poll, 1000)
end

poll()

