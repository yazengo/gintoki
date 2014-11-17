
require('airplay')
require('itunes')
require('dbgsrv')
require('radio')
require('audio')
require('muno')
require('zpnp')
require('prop')
require('ctrl')

handle = function (o, done)
	done = done or function () end
	local fail = function () done{result=1, msg='params invalid'} end
	
	if not o or not o.op or not isstr(o.op) then
		fail()
		return
	end

	if ctrl.setopt(o, done) then return end

	if string.hasprefix(o.op, 'muno.') then
		if not muno.setopt(o, done) then fail() end
	elseif string.hasprefix(o.op, 'alarm.') then
		if not alarm or not alarm.setopt(o, done) then fail() end
	else
		fail()
	end
end

audio.track_stat_change = function (i)
	if i ~= 0 then return end
	local r = muno.audioinfo()
	pnp.notify_event{['audio.info']=r}
end

if input then
	input.cmds = {
		[[ audio.setvol(audio.getvol() - 10); print(audio.getvol()) ]],
		[[ audio.setvol(audio.getvol() + 10); print(audio.getvol()) ]],
		[[ audio.setvol(80); print(audio.getvol()) ]],
		[[ audio.setvol(0); print(audio.getvol()) ]],
		[[ handle{op='muno.request_sync'} ]],
		[[ handle{op='audio.play_pause_toggle'} ]],
		[[ handle{op='audio.play_pause_toggle', eventsrc='inputdev'} ]],
		[[ handle{op='audio.next'} ]],
		[[ handle{op='radio.change_type', type='pandora'} ]],
		[[ handle{op='radio.change_type', type='local'} ]],
		[[ handle{op='radio.change_type', type='slumber'} ]],
		[[ handle{op='radio.change_type', type='douban'} ]],
		[[ handle{op='radio.change_type', type='bbcradio'} ]],
		[[ handle{op='muno.set_poweroff_timeout', timeout=1024} ]],
		[[ handle{op='muno.cancel_poweroff_timeout'} ]],
		[[ handle({op='audio.play', id='2'}, info)]],
		[[ handle({op='audio.play', id='1'}, info)]],
		[[ handle{op='audio.alert', url='testaudios/beep0.5s.mp3'} ]],
		[[ handle{op='breaking.audio', url='testaudios/2s-1.mp3'} ]],
		[[ handle{op='burnin.start'} ]],
		[[ handle{op='burnin.stop'} ]],
		[[ handle({op='burnin.totaltime'}, info) ]],
	}
end

info(os.date(), hostname(), muno.version())

pnp.on_action = function (o, done)
end

airplay.start()
pnp.start()
itunes.start()
ctrl.start()
dbgsrv.start()

handle{op='radio.change_type', type=prop.get('radio.default', 'local')}

