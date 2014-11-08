
h1 = set_interval(function ()
	info('interval')
end, 1000)

set_timeout(function ()
	info('timeout')
end, 1000)

set_timeout(function ()
	clear_interval(h1)
end, 3000)

h2 = set_timeout(function ()
	info('hey')
end, 1000)
clear_timeout(h2)

