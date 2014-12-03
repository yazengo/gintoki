
require('prop')
require('pipe')
require('audio')

local B = {}

local p

B.stop = function ()
	if p then
		p.close()
		p = nil
	end
end

local function updatetm (dur)
	local total = prop.get('burnin.time', 0) + dur
	prop.set('burnin.time', total)
end

B.start = function ()
	B.stop()
	p = pipe.new()

	local c
	c = pipe.copy(audio.noise(), p, 'rw').done(function ()
		updatetm(c.rx() / (44100*4))
	end)

	return p
end

B.totaltime = function ()
	return math.ceil(prop.get('burnin.time', 0))
end

burnin = B

