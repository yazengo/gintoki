
prof_clear()
set_interval(function ()
	info('PROFILE', prof_collect())
	prof_clear()
end, 1000)

