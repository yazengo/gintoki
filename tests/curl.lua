
curl {
	url = 'sugrsugr.com:8083',
	body = cjson.encode{op='pandora.stations_list'},
	done = function (ret)
		local js = cjson.decode(ret) or {}
		info('curlret', js)
	end,
}

