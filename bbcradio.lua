
local B = {}

B.info = function ()
	return {
		type = 'bbcradio'
	}
end

B.json = (function ()
	local js = loadjson('bbcradio.json') or {}
	local r = {}
	for k,v in pairs(js.radios or {}) do
		local s = {
			id = tostring(k),
			title = v.name,
			url = v.uri,
		}
		r[k] = s
	end
	return r
end)()

B.setopt = function (o, done)
	if o.op == 'bbcradio.stations_list' then
		done{radios=B.json}
	elseif o.op == 'bbcradio.station_choose' then
		local i = tonumber(o.id)
		local r = B.json[i]
		if not r then 
			done{result=1}
			return
		end
		B.i = i-1
		if B.next_callback then
			B.next_callback()
		end
		done{result=0}
	end
end

B.next = function ()
	if not B.i then B.i = 0 end
	B.i = (B.i+1) % table.maxn(B.json)
	return B.json[B.i] or {}
end

bbcradio = B

