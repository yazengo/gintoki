
require('prop')
require('localmusic')
require('pandora')
require('douban')
require('bbcradio')

local R = {}

R.info = function ()
	local r = {}
	table.add(r, R.source.info())
	table.add(r, R.song or {})
	return r
end

R.next = function (o)
	if R.inserting then return end
	info('next')

	if not R.source or not R.source.next then return end
	R.song = R.source.next(o)
	if not R.song then return end
	if R.play then R.play(R.song) end
end

R.prev = function (o)
	if R.inserting then return end
	info('prev')

	if not R.source or not R.source.prev then return end
	R.song = R.source.prev(o)
	if not R.song then return end
	if R.play then R.play(R.song) end
end

R.start = function (source, o)
	info("start")
	R.source = source

	source.next_callback = function ()
		if R.source == source then R.next() end
	end

	source.stop_callback = function ()
		if R.source == source then 
			if R.stop then R.stop() end 
		end
	end

	if R.source.start then
		R.source.start()
	end

	if o.autostart ~= false then
		R.next()
	end
end

R.inserting_hook = function (o, done)
	if o.op == 'audio.play_pause_toggle' then
		if o.current then
			audio.pause_resume_toggle{track=2}
			done{result=0}
			return true
		else
			R.stop_inserting()
		end
	elseif o.op == 'audio.pause' then
		if o.current then 
			audio.pause{track=2}
			done{result=0}
			return true
		else
			R.stop_inserting()
		end
	elseif o.op == 'audio.resume' then
		if o.current then 
			audio.resume{track=2}
			done{result=0}
			return true
		else
			R.stop_inserting()
		end
	elseif o.op == 'radio.change_type' then
		R.stop_inserting()
	elseif o.op == 'audio.prev' then
		R.stop_inserting()
	elseif o.op == 'audio.next' then
		R.stop_inserting()
	elseif o.op == 'audio.play' then
		R.stop_inserting()
	end
end

R.setopt_hook = function (o, done)
	if R.inserting and R.inserting_hook(o, done) then
		return true
	end

	if o.op == 'local.toggle_repeat_mode' and R.source == slumbermusic then
		o.op = 'slumber.toggle_repeat_mode'
	end

	if o.op == 'audio.play' then
		if R.source == slumbermusic then o.op = 'slumber.play' end
		if R.source == localmusic then o.op = 'local.play' end
	end

	local ok
	if string.hasprefix(o.op, 'local.') then
		ok = localmusic.setopt(o, done)
	elseif string.hasprefix(o.op, 'pandora.') then
		ok = pandora.setopt(o, done)
	elseif string.hasprefix(o.op, 'bbcradio.') then
		ok = bbcradio.setopt(o, done)
	elseif string.hasprefix(o.op, 'douban.') then
		ok = douban.setopt(o, done)
	elseif string.hasprefix(o.op, 'slumber.') then
		ok = slumbermusic.setopt(o, done)
	elseif o.op == 'radio.insert' then
		R.insert(o.url, o)
		done{result=0}
		ok = true
	elseif o.op == 'radio.change_type' then
		R.change(o)
		done{result=0}
		ok = true
	elseif o.op == 'burnin.start' then
		R.burnin_tm = now()
		R.insert('noise://', {resume=true})
		done{result=0}
		ok = true
	elseif o.op == 'burnin.stop' then
		if R.burnin_tm then
			local elapsed = now() - R.burnin_tm
			local total = prop.get('burnin.time', 0) + elapsed
			prop.set('burnin.time', total)
		end
		R.stop_inserting{resume=true}
		done{result=0}
		ok = true
	elseif o.op == 'burnin.totaltime' then
		done{result=math.ceil(prop.get('burnin.time', 0))}
		ok = true
	end

	return ok
end

R.inserting = false
R.insert = function (url, o)
	R.inserting = true
	audio.pause()
	audio.play {
		url = url,
		track = 2,
		done = function ()
			set_timeout(function () 
				o.normal_end = true
				R.stop_inserting(o)
			end, 0)
		end,
	}
end

R.stop_inserting = function (o)
	if not o.normal_end then
		audio.stop{track=2}
	end
	R.inserting = false
	if o.resume then
		audio.resume()
	end
end

R.name2obj = function (s)
	if s == 'pandora' then
		return pandora
	elseif s == 'local' then
		return localmusic
	elseif s == 'slumber' then
		return slumbermusic
	elseif s == 'bbcradio' then
		return bbcradio
	elseif s == 'douban' then
		return douban
	end
end

R.change = function (o)
	info('radio.change', o)

	local to = R.name2obj(o.type)

	if to and R.source ~= to then
		R.stop()
		R.start(to, o)
		prop.set('radio.default', o.type)
	end
end

R.play = function (song) 
	info('play', song.title)
	audio.play {
		url = song.url,
		done = function (dur) 
			info('playdone')
			radio.next{dur=dur, playdone=true}
		end
	}
end

R.stop = function ()
	audio.stop()
end

radio = R

