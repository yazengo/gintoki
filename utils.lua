
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

if os.getenv('test') then
	local ha = {}
	local i = 0
	emitter_init(ha)
	ha.on('haha', function (a)
		i = i + a
		return 3
	end)
	ha.on('haha', function (a, b)
		i = i + a + b
		return 1
	end)
	ha.on('hehe', function (a, b, c)
		i = i + a + b + c
		return 2
	end)
	ha.emit('haha', 123, 11)
	ha.emit('hehe', 5, 88, 99)
	ha.emit('haha', 123, 1, 1)
	assert(i == 696)

	assert(ha.emit_first('haha', 1,2) == 3)
end

