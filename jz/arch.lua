
arch = {}

arch.poweroff = function ()
end

arch.version = function ()
end

arch.fwupdate = function ()
end

arch.battery = function ()
end

arch.wifi_info = function ()
end

putenv('FWUPDATEZIP', '/mnt/sdcard/update.zip')
putenv('MUSICDIR', '/mnt/sdcard/musics')
putenv('SLUMBERDIR', '/mnt/sdcard/slumbermusics')

require('jz/shairport')

