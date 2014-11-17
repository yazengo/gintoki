
require('ctrl')
require('radio')
require('tts')

local devid = hostuuid()

local P = {}

P.send = function (m)
	m.devid = devid
	curl {
		url = 'dev.muno.avosapps.com',
		reqstr = cjson.encode(m),
		done = function (r)
		end,
	}
end

P.on_msg = function (m)
	if not m.song then return end
	local words = 'audio message ' .. m.song.title .. ' by ' .. m.song.artist

	tts.download(words, function (ttspath)
		ctrl.breaking_radio(radio.new_songlist{
			ttspath,
			m.song.url,
		}, {
			resume = true,
		})
	end)
end

P.on_enter_slave = function ()
end

P.on_slave_msg = function ()
end

P.on_leave_slave = function ()
end

P.enter_master = function ()
	ctrl.radio_que.playevents.add_listener(peersync)
end

P.on_playstart = function (song, o)
	P.send(song)
end

P.on_playdone = function (song, o)
	P.send(song)
end

P.leave_master = function ()
	ctrl.radio_que.playevents.remove_listener(P)
end

peerctrl = P

