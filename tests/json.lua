

print(cjson.encode{
	need_update=true,
	firmwares_list={ {
		release_notes = "Test Fix XXXX",
		date = '2013/1/1',
		version = '0.9.1',
		url = 'http://firmware.sugrsugr.com/a.zip',
	} },
})
