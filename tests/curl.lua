
setloglevel(0)

curl {
	url = 'www.sugrsugr.com/a.img',
	body = cjson.encode{op='pandora.genres_list'},
	tofile = '/tmp/xx',
	done = function (ret, code)
		--local js = cjson.decode(ret) or {}
		info(ret)
		info(code)
	end,
}


