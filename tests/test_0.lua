
-- timeout test

set_timeout(function () 
	print('timeout')
end, 100)
set_timeout(function () 
	print('timeout')
end, 200)
set_timeout(function () 
	print('timeout')
end, 300)
clear_timeout(set_timeout(function () 
	print('timeout')
end, 400))

