
require('prop')

local A = {}

alarm_set = function()
    local a = prop.get('alarm')
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
    prop.set('alarm', a)
end

alarm_on_trigger = function () 
	audio.alert {
		url = 'testaudios/ding.mp3',
		vol = 80,
	}
    alarm_set()
end

A.setopt = function (a, done)
	if a.op == 'alarm.datesync' and a.date then 
		popen {
			cmd = "date -s \"" .. a.date .. "\" -D \"%F %T %z\"" .. " && hwclock -w",
		}
		done{result=0}
		return true
	elseif a.op == 'alarm.alarmset' then
		prop.set('alarm', a.alarms)
		alarm_set()
		done{result=0}
		return true
	elseif a.op == 'alarm.alarmget' then
		done{result=0, alarms=prop.get('alarms')}
		return true
	end
end

alarm_init()

alarm = A

