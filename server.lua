
require('airplay')
require('itunes')
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
		[[ handle{op='audio.play_pause_toggle'} ]],
		[[ handle{op='audio.play_pause_toggle', current=true} ]],
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
		[[ handle{op='radio.insert', url='testaudios/2s-1.mp3', resume=true} ]],
		[[ handle{op='radio.insert', url='testaudios/2s-2.mp3'} ]],
		[[ handle{op='burnin.start'} ]],
		[[ handle{op='burnin.stop'} ]],
		[[ handle({op='burnin.totaltime'}, info) ]],
	}
end

http_server {
	addr = '127.0.0.1',
	port = 9991,
	handler = function (hr)
		local cmd = hr:body()
		local func = loadstring(cmd)
		local r, err = pcall(func)
		local s = ''
		if err then
			s = cjson.encode{err=tostring(err)}
		else
			s = cjson.encode{err=0}
		end
		hr:retjson(s)
	end,
}

http_server {
	port = 8881,
	handler = function (hr)
		local js = cjson.decode(hr:body()) or {}
		handle(js, function (r)
			hr:retjson(cjson.encode(r))
		end)
	end,
}

info('hostname', hostname())
airplay.start()
pnp.start()
ctrl.start()

handle{op='radio.change_type', type=prop.get('radio.default', 'local')}

