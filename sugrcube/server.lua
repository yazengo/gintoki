
require('sugrcube/localmusic')
require('sugrcube/burnin')
require('sugrcube/muno')

require('douban')
require('pandora')
require('bbcradio')

require('audio')
require('playlist')

require('zpnp')

local S = {}

S.setopt_vol = function (o, done)
	if type(o.vol) == 'number' then
		if o.vol > 100 then o.vol = 100 end
		if o.vol < 0 then o.vol = 0 end
		S.vol.sw.setvol(o.vol / 100)

		muno.notify_vol_change(o.vol)
		done(o.vol)
	else
		done(1, 'vol invalid')
	end
end

S.setopt_radio_change = function (o, done)
	local types = {
		['douban'] = douban,
		['bbcradio'] = bbcradio,
		['pandora'] = pandora,
		['local'] = localmusic,
		['slumber'] = slumbermusic,
	}

	local src = types[o.type]
	if src == nil then
		done(1, 'type invalid')
		return
	end

	if src == S.player.src() then
		done()
		return
	end

	S.player.setsrc(src)
	S.sw.setsrc(S.player)
	S.player.resume()

	done()
end

local actions = {
	['audio.volume'] = S.setopt_vol,
	['radio.change_type'] = S.setopt_radio_change,

	['audio.next'] = function (o, done)
		S.player.resume()
		S.player.next()
		done()
	end,

	['audio.prev'] = function (o, done)
		S.player.resume()
		S.player.prev()
		done()
	end,

	['audio.pause'] = function (o, done)
		S.player.pause()
		done()
	end,

	['audio.resume'] = function (o, done)
		S.player.resume()
		done()
	end,

	['audio.pause_play_toggle'] = function (o, done)
		S.player.pause_resume()
		done()
	end,

	['burnin.start'] = function (o, done)
		S.burnin = burnin.src()
		S.sw.breakin(S.burnin)
		burnin.start()
		done()
	end,

	['burnin.stop'] = function (o, done)
		S.sw.stop_breakin(S.burnin)
		burnin.stop()
		done()
	end,

	['burnin.totaltime'] = function (o, done)
		done(burnin.totaltime())
	end,

	['audio.play'] = function (o, done)
		local src = S.player.src()
		if src and src.jump_to then
			src.jump_to(tonumber(o.id))
			done()
		else
			done(1, 'unsupported')
		end
	end,

	['douban.login'] = douban.setopt_login,
	['douban.logout'] = douban.setopt_logout,
	['douban.channel_choose'] = douban.setopt_channel_choose,
	['douban.channels_list'] = douban.setopt_channels_list,
	['douban.stat'] = douban.setopt_stat,
	['douban.rate_like'] = douban.setopt_rate_like,
	['douban.rate_unlike'] = douban.setopt_rate_unlike,
	['douban.rate_toggle_like'] = douban.setopt_rate_toggle_like,
	['douban.rate_ban'] = douban.setopt_rate_ban,

	['pandora.login'] = pandora.setopt_login,
	['pandora.genres_choose'] = pandora.setopt_genres_choose,
	['pandora.genres_list'] = pandora.setopt_genres_list,
	['pandora.station_choose'] = pandora.setopt_station_choose,
	['pandora.stations_list'] = pandora.setopt_stations_list,
	['pandora.rate_like'] = pandora.setopt_rate_like,
	['pandora.rate_ban'] = pandora.setopt_rate_ban,

	['bbcradio.stations_list'] = bbcradio.setopt_stations_list,
	['bbcradio.station_choose'] = bbcradio.setopt_station_choose,

	['muno.request_sync'] = muno.setopt_sync,
	['muno.request_event'] = muno.setopt_event,
	['muno.info'] = muno.setopt_info,
	['muno.set_poweroff_timeout'] = muno.setopt_poweroff,
	['muno.cancel_poweroff_timeout'] = muno.setopt_cancel_poweroff,
	['muno.do_update'] = muno.setopt_do_update,
	['muno.cancel_update'] = muno.setopt_cancel_update,
}

S.handle = function (o, _done)
	_done = _done or function () end
	local done = function (r, err)
		if r == nil then r = {result=0} end
		if type(r) == 'number' then r = {result=r, msg=err} end
		if type(r) ~= 'table' then panic('done() result must be table') end
		r.result = 0
		_done(r)
	end

	set_immediate(function ()
		local f = actions[o.op]
		if f == nil then
			done(1, 'invalid op')
		else
			f(o, done)
		end
	end)
end

S.audio_info = function ()
	local r = {}

	r.stat = S.player.stat()

	local src = S.player.src()
	if src then r.type = src.name end

	if S.player.song then table.add(r, S.player.song) end
	r.url = nil

	r.pos = S.player.pos()
	if r.pos then r.pos = math.ceil(r.pos) end

	r.dur = S.player.dur

	return r
end

S.audio_statchanged = function ()
	pnp.notify_event { ['audio.info'] = S.audio_info() }
end

S.getvol = function ()
	return S.vol.sw.getvol()
end

S.localmusic_nr = function ()
	return table.maxn(localmusic.songs)
end

local function init (calls, done)
	local n = 0
	for _, f in pairs(calls) do
		f(function ()
			n = n + 1
			if n == table.maxn(calls) then
				done()
			end
		end)
	end
end

init({
	slumbermusic.init,
	localmusic.init,
	shairport.init,
}, function ()
	info('server starts')

	S.sw = audio.switcher()

	S.vol = {}
	S.vol.sw = audio.effect()
	S.vol.sw.setvol(0.5)

	S.vol.alarm = audio.effect()

	S.mix = audio.mixer()
	audio.pipe(S.sw, S.vol.sw, S.mix.add())

	S.player = playlist.player().statchanged(S.audio_statchanged)

	audio.pipe(S.mix, audio.out())

	S.player.setsrc(localmusic)

	S.sw.setsrc(S.player)

	shairport.start(function (r)
		S.player.pause()
		S.sw.breakin(r)
	end)

	muno.audio_info = S.audio_info
	muno.getvol = S.getvol
	muno.localmusic_nr = S.localmusic_nr

	pnp.on_action = S.handle
	pnp.start()
end)

if input then input.cmds = {
	[[ server.handle({op='audio.pause'}, info) ]],
	[[ server.handle({op='audio.resume'}, info) ]],
	[[ server.handle({op='audio.next'}, info) ]],
	[[ server.handle({op='audio.prev'}, info) ]],
	[[ server.handle({op='audio.pause_play_toggle'}, info) ]],

	[[ server.handle({op='radio.change_type', type='douban'}, info) ]],

	[[ server.handle({op='burnin.start'}, info) ]],
	[[ server.handle({op='burnin.stop'}, info) ]],
} end

server = S

