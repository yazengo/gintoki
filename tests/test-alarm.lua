
require('audio')
require('cmd')

input.cmds = {
    [[ inputdev_on_event(30) ]],
    [[ handle{op='alarm.datesync', date='2014-10-21 15:31:35 +0000'} ]],
    [[ handle{op='alarm.alarmset', alarms={
        {hour=16, minute=20, ['repeat']=49, enable=true},
        {hour=11, minute=20, ['repeat']=31, enable=true},
        }} ]],
    -- near future
    [[ handle{op='alarm.alarmset', alarms={
        {hour=15, minute=32, ['repeat']=31, enable=true},
        }} ]],
    -- no repeat
    [[ handle{op='alarm.alarmset', alarms={
        {hour=15, minute=33, ['repeat']=0, enable=true},
        }} ]],
    [[ handle{op='alarm.alarmset', alarms={
        {hour=15, minute=30, ['repeat']=0, enable=true},
        }} ]],
    [[ handle{op='alarm.alarmget'} ]],
}

