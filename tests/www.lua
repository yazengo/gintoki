
http_server {
	port = 8080,
	handler = function (r)
		local u = r:url()
		if u == '/' then 
			u = '/index.html'
		end
		local path = 'www' .. u
		r:retfile(path)
	end,
}

http_server {
	port = 8081,
	handler = function (r)
	end,
}

