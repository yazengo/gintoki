
table.add = function (a, ...) 
	for _,t in ipairs{...} do
		for k,v in pairs(t) do
			a[k] = v
		end
	end
	return a
end

table.dump = function (t) 
	for k,v in pairs(t) do
		print(k,v)
	end
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
	for k, v in ipairs{...} do
		if type(v) == 'table' then
			s = s .. cjson.encode(v)
		else
			s = s .. v
		end
		s = s .. ' '
	end
	_info(s)
end

