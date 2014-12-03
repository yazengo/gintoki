
table.add = function (a, ...) 
	for _,t in ipairs{...} do
		for k,v in pairs(t) do
			a[k] = v
		end
	end
	return a
end

table.copy = function (t, filter)
	if filter == nil then
		return table.add({}, t)
	else
		local r = {}
		for k, v in pairs(t) do
			if filter(k, v) then
				r[k] = v
			end
		end
		return r
	end
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

table.count = function (t)
	local n = 0
	for _ in pairs(t) do n = n + 1 end
	return n
end

table.contains = function (table, element)
    for _, value in pairs(table) do
        if value == element then
            return true
        end
    end
    return false
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
panic = function (...) 
	print(ldebug.traceback())
	_log_at(4, 3, ...) 
end

basename = function (s)
	local x = string.gsub(s, '%.[^%.]*$', '')
    x = string.gsub(x, '^.*%/', '')
	return x
end

randomhexstring = function (n)
	local i = 16^n - (16^n)*math.random()
	return string.format('%.' .. n .. 'x', i)
end

math.randomseed(os.time())

urlencode = function (s)
	return string.gsub(s, "([ !*'%(%);:@&=+$,/?%%#%[%]])", function (ch) 
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
isstring = isstr

io.readstring = function (path)
	local f = io.open(path, 'r')
	if not f then return end
	local s = f:read('*a')
	f:close()
	return s
end

io.readnumber = function (path)
	return tonumber(io.readstring(path))
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
	return tonumber(string.sub(sha1_encode(hostname()), -7), 16)
end

system = function (cmd, done)
	done = done or function () end
	popen {
		cmd = cmd,
		done = function (r, code)
			done(code)
		end,
	}
end

loadconfig = function (path)
	local t = {}
	local mt = { __newindex = function (_,k,v) t[k] = v end }
	local oldmt = getmetatable(_ENV, mt)
	setmetatable(_ENV, mt)
	pcall(loadfile(path))
	setmetatable(_ENV, oldmt)
	return t
end

version_cmp = function (tpl, a, b)
	local ta = {}
	local tb = {}
	ta[1],ta[2],ta[3],ta[4] = string.match(a, tpl)
	tb[1],tb[2],tb[3],tb[4] = string.match(b, tpl)
	if table.maxn(ta) == 0 or table.maxn(tb) == 0 then return end
	if table.maxn(ta) ~= table.maxn(tb) then return end
	for i=1,4 do
		if ta[i] == nil then break end
		local v = tonumber(ta[i]) - tonumber(tb[i])
		if v ~= 0 then return v end
	end
	return 0
end

