
local D = {}

D.start = function ()
	http_server {
		addr = '127.0.0.1',
		port = 9991,
		handler = function (hr)
			local cmd = hr:body()
			local func = loadstring(cmd)
			local r, err = pcall(func)
			local s = ''
			if err then
				s = cjson.encode{err=tostring(err)}
			else
				s = cjson.encode{err=0}
			end
			hr:retjson(s)
		end,
	}

	http_server {
		port = 8881,
		handler = function (hr)
			local js = cjson.decode(hr:body()) or {}
			handle(js, function (r)
				hr:retjson(cjson.encode(r))
			end)
		end,
	}
end

dbgsrv = D

