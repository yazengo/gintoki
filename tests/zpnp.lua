
zpnp_start()
zpnp_notify()
zpnp_setopt{uuid=0x1133, name=hostname()}

zpnp_on_recv = function (r, done) 
	info(r)
	done('world')
end

