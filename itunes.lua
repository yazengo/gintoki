
local I = {}

I.handler = function (r)
	info('url', r:url())
	info('method', r:method())
	local upload_dir = jz_itunes_dir or 'www/'
	if r:url() == "/upload" then
		if r:method() == 1 then
			r:retfile('www/upload.html')
		elseif r:method() == 3 then
			local _, _, filename = string.find(r:body(), 'filename="(.-)"')
			if not filename then
				r:retjson(cjson.encode{result = 1})
				return
			end
			r:savebody(upload_dir .. filename)
			r:retjson(cjson.encode{result = 0})
			localmusic.addlist(upload_dir .. filename)
		end
	end
end

I.start = function ()
	http_server {
		port = 8883,
		handler = function (r)
			I.handler(r)
		end,
	}
end

itunes = I

