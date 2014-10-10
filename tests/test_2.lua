
-- emitter test

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

