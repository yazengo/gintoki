
require('cmd')

loop = function ()
	audio.play {
		url = 'musics/SHE.mp3',
		done = function ()
			loop()
		end,
	}
end
loop()

linear = function ()
	local t = { 0 }
	local base = 100

	for i = 1,100 do
		t[i+1] = i
	end
	return {
		tblvals = t,
		tblbase = base,
	}
end

log2 = function ()
	local t = { 0 }
	local base = 4096
	local maxdb = 64

	for i = 1,100 do
		local db = -3*(math.pow(2, 4.4*((100-i)/100))-1)
		t[i+1] = math.pow(10, db/20.0)*base
	end

	return {
		tblvals = t,
		tblbase = base,
	}
end

dump = function (o)
	local s = ''
	for k,v in ipairs(o.tblvals) do
		s = s .. (math.floor(v) .. ',')
	end
	print(s)
end

pcm_setopt(log2())
dump(log2())

input.cmds = {
	[[ audio.setvol(10) ]],
	[[ audio.setvol(20) ]],
	[[ audio.setvol(30) ]],
	[[ audio.setvol(40) ]],
	[[ audio.setvol(50) ]],
	[[ audio.setvol(60) ]],
	[[ audio.setvol(70) ]],
	[[ audio.setvol(80) ]],
	[[ audio.setvol(90) ]],
	[[ audio.setvol(100) ]],

	[[ pcm_setopt(linear())  ]]
}

