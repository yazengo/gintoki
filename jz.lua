
require('alarm')

fwupdate_root = '/mnt/sdcard/'
fwupdate_recovery = function ()
	pnp.stop()
	system('sync && reboot_recovery')
end

arch_poweroff = function ()
end

prop_filepath = '/mnt/sdcard/prop.json'
jz_itunes_dir = '/mnt/sdcard/musics/'
localmusic_dir = '/mnt/sdcard/musics/'
slumbermusic_dir = '/mnt/sdcard/slumbermusics/'

arch_version = loadconfig('/usr/app/version')

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

local gsensor_prev = say_and_do('prev')
local gsensor_next = say_and_do('next')

getssid = function (done)
	popen {
		cmd = 'wpa_cli status', 
		done = function (r, code) 
			local ssid
			for k,v in string.gmatch(r, 'ssid=([^\n]+)') do
				ssid = k
			end
			for k,v in string.gmatch(r, 'ip_address=([^\n]+)') do
				ipaddr = k
			end
			done(ssid, ipaddr)
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
		handle{op='audio.play_pause_toggle', eventsrc='inputdev'}
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
		getssid(function (ssid, ip) 
			zpnp_setopt{name=hostuuid() .. '@' .. ip}
			pnp.online()
		end)
	end

	-- network down
	if e == 37 then
		info('network down')
	end

	if e == 50 then
		audio.alert {
			url = 'testaudios/received.mp3',
			vol = 20,
		}
		info('wifi config receive')
	end

	if e == 51 then
		audio.alert {
			url = 'testaudios/association.mp3',
			vol = 20,
		}
		info('wifi association')
	end

	if e == 52 then
		audio.alert {
			url = 'testaudios/disassociation.mp3',
			vol = 20,
		}
		info('wifi disassociation')
	end

	if e == 53 then
		audio.alert {
			url = 'testaudios/connected_fail.mp3',
			vol = 20,
		}
		info('wifi connected fail')
	end

	if e == 54 then
		audio.alert {
			url = 'testaudios/wifi_unknown_err.mp3',
			vol = 20,
		}
		info('wifi unknown error')
	end

	if e == 55 then
		audio.alert {
			url = 'testaudios/association_but_dns_fail.mp3',
			vol = 20,
		}
		info('wifi association, but DNS fail')
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

	if e == 33 or e == 40 or e == 41 then
		alarm.cancel()
	end

end

hostname = function ()
	local cid = io.readstring('/sys/devices/platform/jz4775-efuse.0/chip_id')
	if not cid then
		cid = randomhexstring(7)
	end
	return cid
end

local vol = io.readnumber('/sys/module/adc_volume_driver/drivers/platform:jz4775-hwmon/jz4775-hwmon.0/volume')
inputdev_init() 
setvol(vol)

