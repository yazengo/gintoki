
zpnp_start()
zpnp_setopt{uuid=0x1133, name=hostname()}
zpnp_notify()
zpnp_notify('hello_first_notify')

zpnp_on_action = function (r, done) 
	info(r)
	done('world')
end

zpnp_on_subscribe = function (r, done)
	info('subscribe')
	done('hello_subcribe')
end

set_interval(function ()
	zpnp_notify()
end, 300)

