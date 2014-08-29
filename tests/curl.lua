
setloglevel(0)

curl {
	url = 'www.sugrsugr.com',
	body = cjson.encode{op='pandora.genres_list'},
	done = function (ret, code)
		--local js = cjson.decode(ret) or {}
		info(ret)
		info(code)
	end,
}
--[[
local task = curl {
	url = 'www.sugrsugr.com',
	body = cjson.encode{op='pandora.genres_list'},
	done = function (ret, code)
		--local js = cjson.decode(ret) or {}
		info(ret)
		info(code)
	end,
}
--]]


