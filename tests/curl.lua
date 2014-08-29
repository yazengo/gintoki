
setloglevel(0)

curl {
	url = 'firmware.sugrsugr.com/info',
	done = function (ret, code)
		info(cjson.encode(ret))
	end,
}


