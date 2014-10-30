
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

ldebug = debug

_log_at = function (level, caller_at, ...) 
	local di = ldebug.getinfo(caller_at)

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

	_log(level, di.name, di.short_src, di.currentline, s)
end

debug = function (...) _log_at(0, 3, ...) end
info = function (...) _log_at(1, 3, ...) end
panic = function (...) _log_at(4, 3, ...) end

os.basename = function (s)
	local x = string.gsub(s, '%.[^%.]*$', '')
	return x
end

math.randomseed(os.time())

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

readint = function (path)
	local f = io.open(path, 'r')
	if not f then return 0 end
	local s = f:read('*a')
	f:close()
	return tonumber(s)
end

oncer = function () 
	return {
		n = 0,
		last_n = 0,
		interval = 1000,

		timeout = function (self)
			if self.n == 0 then
				if self.done then self.done() end
				self.last_n = 0
				self.running = nil
			else
				self.last_n = self.n
				self.n = 0
				set_timeout(function () self:timeout() end, self.interval)
			end
		end,

		update = function (self)
			if self.running then
				self.n = self.n + 1
			else
				self.running = true
				set_timeout(function () self:timeout() end, self.interval)
			end
		end,
	}
end

function queue()
	return {
		first = 0, 
		last = -1,

		pushleft = function (list, value)
			local first = list.first - 1
			list.first = first
			list[first] = value
		end,

		pushright = function (list, value)
			local last = list.last + 1
			list.last = last
			list[last] = value
		end,

		popleft = function (list)
			local first = list.first
			if first > list.last then return nil end
			local value = list[first]
			list[first] = nil    -- to allow garbage collection
			list.first = first + 1
			return value
		end,

		popright = function (list)
			local last = list.last
			if list.first > last then return nil end
			local value = list[last]
			list[last] = nil     -- to allow garbage collection
			list.last = last - 1
			return value
		end,

		len = function (list)
			return list.last - list.first + 1
		end,
	}
end

hostuuid = function ()
	return tonumber(string.sub(sha1_encode(hostname()), -8), 16)
end

