
setloglevel(0)

curl {
	url = 'www.youtube.com',
	proxy = 'localhost:8888',
	done = function (ret, stat)
		info(ret, stat)
	end,
}

