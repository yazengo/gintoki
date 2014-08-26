
local task = curl {
	url = 'sugrsugr.com:8083',
	body = cjson.encode{op='pandora.genres_list'},
	done = function (ret)
		local js = cjson.decode(ret) or {}
		info('curlret', string.len(ret), js)
	end,
}

task.cancel()

