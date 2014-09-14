
table.add = function (a, ...) 
	for _,t in ipairs{...} do
		for k,v in pairs(t) do
			a[k] = v
		end
	end
	return a
end

table.copy = function (a) 
	return table.add({}, a)
end

table.append = function (a, ...)
	local i = table.maxn(a) + 1

	for _,b in ipairs{...} do
		if type(b) == 'table' then
			for k,v in ipairs(b) do
				if type(k) == 'number' then
					a[i] = v
					i = i+1
				end
			end
		else
			a[i] = b
			i = i+1
		end
	end
	return a
end

string.split = function(s, p)
	if not p then p = ' \t' end
	local rt = {}
	string.gsub(s, '[^'..p..']+', function(w) table.insert(rt, w) end )
	return rt
end

string.hasprefix = function (a, pref)
	return string.sub(a, 1, string.len(pref)) == pref
end

clear_timeout = function (t)
	if t then _G[t] = nil end
end

emitter_init = function (t)
	t.emit_targets = {}
	t.on = function (name, func) 
		if t.emit_targets[name] == nil then
			t.emit_targets[name] = {}
		end
		table.insert(t.emit_targets[name], func)
	end
	t.emit = function (name, ...) 
		for _, f in pairs(t.emit_targets[name] or {}) do
			f(...)
		end
	end
	t.emit_first = function (name, ...)
		for _, f in pairs(t.emit_targets[name] or {}) do
			return f(...)
		end
	end
end

_info = function (caller_at, ...) 
	local s = ''
	local t = {...}
	for i = 1,table.maxn(t) do
		local v = t[i]
		if type(v) == 'table' then
			s = s .. (cjson.encode(v) or '(unjsonable)')
		elseif type(v) == 'boolean' then
			if v then s = s .. 'true'
			else s = s .. 'false' end
		elseif v == nil then
			s = s .. 'nil'
		else
			s = s .. v
		end
		s = s .. ' '
	end

	local di = debug.getinfo(caller_at)

	_log(1, di.name, di.short_src, di.currentline, s)
end

info = function (...) _info(3, ...) end

os.basename = function (s)
	local x = string.gsub(s, '%.[^%.]*$', '')
	return x
end

prop = {}

prop.fname = 'cfg.json'

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
end

prop.load = function ()
	prop.cfg = loadjson(prop.fname) or {}
end

prop.save = function ()
	savejson(prop.fname, prop.cfg or {})
end

math.randomseed(os.time())

logger = function (name)
	return function (...) 
		_info(3, name, ...)
	end
end

urlencode = function (s)
	return string.gsub(s, "([!*'%(%);:@&=+$,/?%%#%[%]])", function (ch) 
		return string.format('%%%.2X', string.byte(ch))
	end)
end

savejson = function (fname, js) 
	local f = io.open(fname, 'w+')
	f:write(cjson.encode(js))
	f:close()
end

loadjson = function (fname) 
	local f = io.open(fname, 'r')
	if f == nil then return {} end
	local s = f:read('*a')
	f:close()
	return cjson.decode(s) or {}
end

encode_params = function (p)
	local r = {}
	for k,v in pairs(p) do
		table.insert(r, k .. '=' .. urlencode(v))
	end
	return table.concat(r, '&')
end

totable = function (t, default)
	if not t or type(t) ~= 'table' then
		return default or {}
	end
	return t
end

istable = function (s)
	if not s or type(s) ~= 'table' then
		return false
	end
	return true
end

tostr = function (s, default)
	if not s or type(s) ~= 'string' then
		return default or ''
	end
	return s
end

isstr = function (s)
	if not s or type(s) ~= 'string' then
		return false
	end
	return true
end

