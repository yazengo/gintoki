
require('pipe')

local S = {}

local do_copy
local loop
local spawn
local c
local proc
local fake
local ppath = '/var/run/shairport-audio'

do_copy = function (sp)
	local fake = pdirect()
	info('started')

	c = pipe.copy(sp, fake, 'b').started(function ()
		c.close()
		info('started')
		pipe.copy(sp, fake, 'brw').done(function (reason)
			if reason == 'w' then pexec('pkill shairport') end
			loop()
		end)
		S.handler(fake)
	end)
end

loop = function ()
	info('waiting conn')
	pfifo_open(ppath, function (sp)
		do_copy(sp)
	end)
end

spawn = function ()
	local r = pexec(string.format('shairport -o pipe %s', ppath), 'c')
	proc = r[1]
	proc.exit_cb = function (code)
		info('exit', code)
		set_timeout(spawn, 1000)
	end
end

S.start = function (handler)
	S.handler = handler
	spawn()
	loop()
end

shairport = S

