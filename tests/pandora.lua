
setloglevel(0) 

url = '://tuner.pandora.com/services/json/?'
params = {
	method = 'auth.partnerLogin',
}
data = {
	deviceModel = 'android-generic',
	username = 'android',
	password = 'AC7IBG09A3DTSYM4R41UJWL07VLN8JI7',
	version = '5',
}

encode_params = function (p)
	local r = ''
	for k,v in pairs(p) do
		r = r .. k .. '=' .. v .. '&'
	end
	return r
end

curl {
	proxy = 'localhost:8888',
	url = 'https' .. url .. encode_params(params),
	content_type = 'text/plain',
	reqstr = cjson.encode(data),
	done = function (ret) 
		print(ret)
	end,
}
