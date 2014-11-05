
test1 = function ()
	curl {
		url = 'www.sugrsugr.com',
		done = function (ret, stat)
			info('total', #ret, 'bytes')
			test2()
		end,
		on_header = function (k, v)
			info(k, v)
		end,
	}
end

test2 = function ()
	local c = curl {
		url = 'sugrsugr.com/a.img',
		done = function (ret, stat)
			info('stat', stat)
		end,
	}

	i = 0
	poll = function ()
		info(c:stat())
		i = i + 1
		if i >= 5 then
			c:cancel()
		end
		if i >= 6 then return end
		set_timeout(poll, 1000)
	end

	poll()
end

test1()

