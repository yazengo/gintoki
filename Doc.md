Gintoki C API
----

# Pipe

	pipe_t *p = pipe_new(PDIRECT);
	// 3 types of pipe
	// PSTREAM_SRC, PSTREAM_SINK, PDIRECT

	void read_done(pipe_t *p, pipebuf_t *pb);	
	pipe_read(p, read_done);
	
	void write_done(pipe_t *p, int stat);
	pipe_write(p, write_done);
	
	pipebuf_t *p = pipebuf_new();
	pipebuf_unref(p);

Gintoki Lua API
----

# Pipe

	c = pipe.copy(from, to, 'brw')
	-- 'b' block copy
	-- 'r' close read when finish
	-- 'w' close write when finish
	
	c.close()
	-- finish copy immediatly
	
	c.done(function (reason) 
	end)
	-- reason == 'w' write end closed
	-- reason == 'r' read end closed
	-- reason == 'c' user closed
	
	pipe.grep(src, 'word', function (line)
	end)
	-- read lines from src and grep the line contains 'word'
	
	pipe.readall(src, function (bigstr)
	end)
	-- read all lines from src
	
# Audio

	audio.noise()
	-- random noise signal src
	
	a = audio.effect()
	-- include volume, fadein, fadeout, eq
	
	a.setvol(1.0)
	-- set volume. range from 0.0 ~ 1.0

	a.getvol()
	-- get current volume
	
	a.fadein(300)
	-- volume change from 0.0 to current volume in 300ms
	
	a.fadeout(300)
	-- volume change from current volume to 0.0 in 300ms
	
	a.cancel_fade()
	-- cancel fadein/fadeout reset to current volume
	