
local A = {}

airplay_on_start = function ()
	radio.insert('airplay://')
end

A.start = function ()
	popen {
		cmd = 'which shairport',
		done = function (r, code)
			if code == 0 then
				airplay_start('Muno_' .. hostname())
			end
		end,
	}
end

airplay = A

