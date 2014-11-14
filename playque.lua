
playque = function (track, radio)
	local P = {}

	P.play = function (song)
		P.song = song
		P.playing = true

		audio.play {
			track = track,
			url = song.url,
			done = function ()
				set_timeout(function ()
					P.playing = false
					P.loop({normal_end=true})
				end, 0)
			end,
		}
	end

	P.next = function ()
		if not P.playing then
			radio.next()
		end
	end

	P.pause_resume_toggle = function ()
		if P.paused then
			P.resume()
		else
			P.pause()
		end
	end

	P.pause = function ()
		if P.paused then return end
		P.paused = true
		if P.playing then
			audio.pause{track=track}
		end
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

	P.loop = function (o)
		P.song = nil
		radio.next(o, P.on_song)
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

	radio.on_skip = function ()
		if P.playing then
			audio.stop{track=track}
		end
	end

	radio.on_stop = function ()
		radio.on_skip()
		P.on_closed()
	end

	return P
end

