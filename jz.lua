
require('alarm')

fwupdate_root = '/mnt/sdcard/'
fwupdate_recovery = function ()
	system('sync && reboot_recovery')
end

local say_and_do = function (k)
	return function ()
		if not (radio.source and radio.source[k]) and k == 'prev' then
			k = 'next'
		end
		audio.alert {
			url = 'testaudios/' .. k .. '.mp3',
			done = function ()
				handle { op = 'audio.' .. k}
			end,
		}
	end
end

gsensor_prev = say_and_do('prev')
gsensor_next = say_and_do('next')

getssid = function (done)
	popen {
		cmd = 'wpa_cli status', 
		done = function (r, code) 
			local ssid
			for k,v in string.gmatch(r, 'ssid=([^\n]+)') do
				ssid = k
			end
			done(ssid)
		end,
	}
end

local vol_oncer = oncer()
vol_oncer.interval = 700
vol_oncer.done = function ()
	muno.notify_vol_change(vol_oncer.vol)
end

local setvol = function (v)
	local vol = math.ceil(100*v/15)
	info('inputdev: vol', v, '->', vol)
	audio.setvol(vol)
	vol_oncer:update()
	vol_oncer.vol = vol
end

local keypress = {
	n = 0,
	mode = 'dblquick', -- single/dblquick/dblwait

	interval = 700,

	hit = function (p)
		info('hit')

		if p.mode == 'none' then
			p:click()
		elseif p.mode == 'dblquick' then
			p.n = p.n + 1
			if p.n == 1 then
				p:click()
				p.timer = set_timeout(function () 
					p.n = 0
				end, p.interval)
			elseif p.n == 2 then
				clear_timeout(p.timer)
				p:dblclick()
				p.n = 0
			end
		elseif p.mode == 'dblwait' then
			p.n = p.n + 1
			if p.n == 1 then
				p.timer = set_timeout(function () 
					p:click()
					p.n = 0
				end, p.interval)
			elseif p.n == 2 then
				clear_timeout(p.timer)
				p:dblclick()
				p.n = 0
			end
		end
	end,

	click = function (p)
		info('click')
		handle{op='audio.play_pause_toggle', current=true}
	end,

	dblclick = function (p)
		info('dblclick')
	end,
}

inputdev_on_event = function (e) 
	info('e=', e)

	if e == 33 then
		info('inputdev: keypress')
		keypress:hit()
	end

	if e == 38 then
		info('inputdev: volend')
		audio.setvol(0)
		muno.notify_vol_change(0)
	end

	if e == 332 then
		info('long press')
		audio.alert {
			url = 'testaudios/hello-muno.mp3',
			vol = 0,
		}
	end

	if e == 168 then
		audio.alert {
			url = 'testaudios/shake.mp3',
			vol = 0,
		}
	end

	-- network up
	if e == 36 then
		audio.alert {
			url = 'testaudios/connected.mp3',
			vol = 20,
		}
		info('network up')
		upnp.start()
	end

	-- network down
	if e == 37 then
		info('network down')
		upnp.stop()
	end

	if e == 40 then
		gsensor_next()
	end

	if e == 41 then
		gsensor_prev()
	end

	if e >= 0 and e <= 15 then
		setvol(e)
	end
end

hostname = function ()
	local fp = io.open('macaddr', 'r')
	local mac = fp:read('*a')
	fp:close()

	mac = string.gsub(mac, '\n', '')

	local name = string.sub(mac, -6)
	hostname = function ()
		return name
	end
end

local vol = readint('/sys/module/adc_volume_driver/drivers/platform:jz4775-hwmon/jz4775-hwmon.0/volume')
inputdev_init() 
setvol(vol)

