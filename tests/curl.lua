
setloglevel(0)

curl {
	url = 'www.qq.com',
	retfile = '/tmp/a.html',
	done = function (ret, stat)
		info(stat)
	end,
}

--[[

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
	if i >= 5 then
		c.cancel()
	end
	if i >= 6 then return end
	set_timeout(poll, 1000)
end

poll()

curl {
	url = 'sugrsugr.com',
	done = function (ret, stat)
		assert(stat.code == 301)
	end,
}

curl {
	url = 'sugrsugr.com',
	retfile = '/tmp/',
	done = function (ret, stat)
		info(stat)
	end,
}

]]--
