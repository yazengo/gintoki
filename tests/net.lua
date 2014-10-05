
--[[
http_server {
	port = 1111,
	handler = function (req, resp)
		req:url() -- '/getpath'
		resp:retfile('path/to/file')
		resp:retjson('{...}')
		resp:ret404()
	end,
}
]]--

udp_server {
	port = 8881,
	handler = function (req, resp)
		info(req)
		resp:ret(cjson.encode{ hello=1 })
	end,
}

tcp_server {
	port = 8882,
	handler = function (req, resp)
		info(req)
		resp:ret('hello world\n')
	end,
}

http_server {
	port = 8883,
	handler = function (r)
		info('url', r:url())
		info('body', r:body())
		r:retjson('haha\n')
	end,
}

http_server {
	port = 8884,
	handler = function (r)
		r:retfile(string.sub(r:url(), 2))
	end,
}

http_server {
	port = 8885,
	handler = function (r)
	end,
}

