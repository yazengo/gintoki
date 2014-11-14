
require('radio')

playque = function (track, radio)
	local P = {}

	P.closed = function (done)
		P.on_closed = done
		return P
	end

	P.pause = function ()
		if P.paused then return end
		P.paused = true
		if P.playing then
			audio.pause{track=track}
		end
	end

	P.play = function (song)
		P.song = song
		P.playing = true

		audio.play {
			track = track,
			url = song.url,
			done = function ()
				P.playing = false
				P.loop()
			end,
		}
	end

	P.resume = function ()
		if not P.paused then return end
		P.paused = false
		if P.playing then
			audio.resume{track=track}
		elseif P.pending_func then
			P.pending_func()
			P.pending_func = nil
		end
	end

	P.on_song = function (song)
		local func = function ()
			P.play(song)
		end
		if P.paused then
			P.pending_func = func
		else
			func()
		end
	end

	P.loop = function ()
		P.song = nil
		radio.next(P.on_song)
		return P
	end

	P.stop = function ()
		radio.stop()
		P.on_closed()
	end

	P.start = function ()
		P.loop()
		return P
	end

	return P
end

local C = {}

C.radio_que = playque(0, radio).start()

C.breaking_event = function ()
	if C.breaking_n then
	end
end

C.breaking_radio = function (name, radio, done)
	if C.breaking_que then
		C.breaking_que.stop()
	end

	local p = playque(2, radio)
	p.closed(function ()
		done()
		C.breaking_n = C.breaking_n - 1
		C.breaking_event()
	end)
	p.name = name
	p.start()

	C.breaking_n = C.breaking_n + 1
	C.breaking_que = p
	C.breaking_event()

	return p
end

ctrl = C

