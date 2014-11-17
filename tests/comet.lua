
require('comet')

comet {
	url = 'push.sugrsugr.com:8880/sub/111',
	handler = function (r)
		info(r)
	end,
}

