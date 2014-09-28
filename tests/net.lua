

--[[

http_server {
	port = 1111,
	handler = function (req, resp)
		resp:return_file('path', 'xx', file')
		resp:return('{...}')
	end,
}
]]--

udp_server {
	port = 8881,
	handler = function (req, resp)
		info(req)
		resp:ret(cjson.encode{
			ip = netinfo_ip()
		})
	end,
}

tcp_server {
	port = 8882,
	handler = function (req, resp)
		info(req)
		resp:ret('hello world\n')
	end,
}

