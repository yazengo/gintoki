
prof_clear()
set_interval(function ()
	info('== profile ==')
	info(prof_collect())
	prof_clear()
end, 1000)

