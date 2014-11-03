
require('cmd')

zpnp_start()
zpnp_setopt { 
	uuid = 0x1133, 
	name = hostname(),
}
zpnp_notify()

zpnp_on_action = function (r, done) 
	info(r)
	done('world')
end

input.cmds = {
	[[ zpnp_stop() ]],
	[[ zpnp_start() ]],
	[[ zpnp_notify() ]],
	[[ zpnp_notify('hello_notify') ]],
}

