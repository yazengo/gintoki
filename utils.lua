
table.add = function (a, ...) 
	for _,t in ipairs{...} do
		for k,v in pairs(t) do
			a[k] = v
		end
	end
	return a
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

info = function (...) 
	local s = ''
	local t = {...}
	for i = 1,table.maxn(t) do
		local v = t[i]
		if type(v) == 'table' then
			s = s .. cjson.encode(v)
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
	_info(s)
end

os.basename = function (s)
	local x = string.gsub(s, '%.[^%.]*$', '')
	return x
end

prop = {}

prop.load = function ()
end

prop.save = function ()
end

math.randomseed(os.time())

