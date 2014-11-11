
require('prop')

local A = {}

local function alarm_set_next()
    local a = prop.get('alarms')
    if not a then return end
    local n = 0
    local min = 0
    info("Alarm set")
    for _,k in ipairs(a) do
        if k.enable == true then
            n = alarm_next(k.hour, k.minute, k['repeat'])
            if n <= 0 then
                k.enable = false
            end

            if min == 0 then
                min = n
            elseif n < min and n > 0 then
                min = n
            end
        end
    end
    alarm_start(min)
    -- refresh enable status
    prop.set('alarms', a)
end

local function alarm_fade(svol, step, evol)
    local vol = svol
    if not evol or evol > 100 then evol = 100 end
    local fadefunc = function ()
        if vol + step > evol then
            vol = evol
        else
            vol = vol + step
        end
        return vol
    end
    return fadefunc
end

local alarm_cancel = false

local function alarm_repeat (url)
	if alarm_cancel then return end
    audio.play {
        url = url,
        track = 3,
        done = function ()
            info("Alarm repeat")
            alarm_repeat(url)
        end,
    }
end

local function alarm_trigger(o)
    alarm_cancel = false
    if o.fadein then
        audio.setvol(20)
        local f = alarm_fade(20, 5)
        interval = set_interval(function ()
            vol = f()
            audio.setvol(vol)
            info("Alarm setvol:", vol)
            if vol >= 60 then
                clear_interval(interval)
            end
        end, 2000)
    end

    alarm_repeat(o.url)

    set_timeout(function()
        alarm_cancel = true
        audio.pause{track = 3}
    end, o.timeout)
end

alarm_on_trigger = function () 
    audio.pause()
    alarm_trigger { url = 'testaudios/10s-2.mp3', timeout = 1000*60, fadein = true }
    alarm_set_next()
end

A.setopt = function (a, done)
	if a.op == 'alarm.datesync' and a.date then 
		popen {
			cmd = 'date -s "' .. a.date .. '" -D "%F %T %z"' .. ' && hwclock -w',
		}
		done{result=0}
		return true
	elseif a.op == 'alarm.alarmset' then
		prop.set('alarms', a.alarms)
		alarm_set_next()
		done{result=0}
		return true
	elseif a.op == 'alarm.alarmget' then
		done{result=0, alarms=prop.get('alarms')}
		return true
	end
end

alarm_init()

alarm = A

