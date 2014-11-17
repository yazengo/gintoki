
--
-- ctrl <-> playque <-> radio
--

playque = function (track, radio)
	local P = {}

	P.radio = radio

	P._subscribe = {}

	P.subscribe = function (name, func)
		local l = P._subscribe[name] or {}
		l[func] = func
		P._subscribe[name] = l
	end

	P.unsubscribe = function (name, func)
		local l = P._subscribe[name] or {}
		l[func] = nil
		P._subscribe = l
	end

	P.emit = function (name, ...)
		local l = P._subscribe[name] or {}
		for _, func in pairs(l) do
			func(...)
		end
	end

	P.play = function (song)
		P.song = song
		P.playing = true
		P.emit('playstart', song, {})
		audio.play {
			track = track,
			url = song.url,
			done = function (pos)
				set_timeout(function ()
					info('done')
					local o = {normal_end=true, pos=pos}
					P.playing = false
					P.loop(o)
					P.emit('playend', song, o)
				end, 0)
			end,
		}
	end

	P.next = function ()
		if P.playing then audio.stop{track=track} end
	end

	P.change = function (radio)
		if P.radio then
			P.radio.cancel_next()
			P.radio_exit()
		end
		P.radio_init(radio)

		if P.playing then 
			audio.stop{track=track} 
		else
			P.loop()
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
		P.emit('pause')
		P.paused = true
		if P.playing then
			audio.pause{track=track}
		end
	end

	P.resume = function ()
		if not P.paused then return end
		P.emit('resume')
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
		P.radio.next(o, P.on_song)
		return P
	end

	P.do_close = function ()
		if not P.closed then
			P.closed = true
			if P.on_closed then P.on_closed() end
		end
	end

	P.close = function ()
		P.radio.cancel_next()
		if P.playing then audio.stop{track=track} end
		P.do_close()
	end

	P.radio_init = function (radio)
		P.radio = radio
		P.radio.on_skip = function ()
			if P.playing then audio.stop{track=track} end
		end
		P.radio.on_closed = function ()
			if P.playing then audio.stop{track=track} end
			P.do_close()
		end
		P.radio.cursong = function ()
			return P.song
		end
	end

	P.radio_exit = function ()
		P.radio.on_skip = nil
		P.radio.on_closed = nil
	end

	if radio then
		P.radio_init(radio)
		P.loop()
	end

	return P
end

