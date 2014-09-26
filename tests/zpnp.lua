
zpnp_start()
zpnp_setopt{uuid=0x1133, name=hostname()}
zpnp_notify()

zpnp_on_recv = function (r, done) 
	info(r)
	done('world')
end

set_interval(function ()
	zpnp_notify('hello_notify')
end, 3000)

