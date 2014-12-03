
require('prop')
require('audio')

local B = {}

B.stop = function ()
	if B.tm then 
		local elapsed = now() - B.tm
		local total = prop.get('burnin.time', 0) + elapsed
		prop.set('burnin.time', total)
	end
end

B.start = function ()
	B.tm = now()
end

B.totaltime = function ()
	return math.ceil(prop.get('burnin.time', 0))
end

B.src = function ()
	return audio.noise()
end

burnin = B

