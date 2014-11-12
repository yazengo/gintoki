
prop = {}

prop.fname = prop_filepath or 'prop.json'

-- prop.get('upnp.name', '')
prop.get = function (pk, default)
	local a = string.split(pk, '.')
	local v = prop.cfg
	for _, k in pairs(a) do
		v = v[k]
		if not v then break end
	end
	return v or default
end

-- prop.set('upnp.name', 'haha')
prop.set = function (pk, pv)
	local a = string.split(pk, '.')
	local n = table.maxn(a)
	local v = prop.cfg
	for i, k in pairs(a) do
		if i == n then
			v[k] = pv
			break
		end
		if not v[k] or type(v[k]) ~= 'table' then 
			v[k] = {}
		end
		v = v[k]
	end
	prop.save()
end

prop.load = function ()
	prop.cfg = loadjson(prop.fname) or {}
end

prop.save = function ()
	savejson(prop.fname, prop.cfg or {})
end

prop.load()

