Gintoki C API
----

# Pipe

	pipe_t *p = pipe_new(L, loop);
	p->type = PSTREAM_SRC;
	// types of pipe:
	//   PSTREAM_SRC, PSTREAM_SINK, PDIRECT

	void read_done(pipe_t *p, pipebuf_t *pb);	
	pipe_read(p, read_done);
	
	void write_done(pipe_t *p, int stat);
	pipe_write(p, write_done);
	
# PipeBuf

	pipebuf_t *p = pipebuf_new();
	pipebuf_unref(p);

Gintoki Lua API
----

# Audio Streaming

## Basic audio src/sink

	audio.out()
	-- audio output sink

	audio.noise()
	-- noise generator src
	
## Audio effect

	a = audio.effect()
	-- include volume, fadein, fadeout, eq
	
	vol = a.setvol(1.0)
	-- set volume. range from 0.0 to 1.0

	vol = a.getvol()
	-- get current volume
	
	a.fadein(300)
	-- volume change from 0.0 to current volume in 300ms
	
	a.fadeout(300)
	-- volume change from current volume to 0.0 in 300ms
	-- keep volume 0.0 until cancel_fade() is called
		
	a.cancel_fade()
	-- cancel fadein/fadeout reset to current volume
	

## Stream switcher

	sw = pipe.switcher()
	
	sw.setsrc(p)
	-- set current playing stream to p. the old one won't be closed
	
	b = sw.breakin(breakin_stream, interrupted)
	-- interrupt current playing stream and play breakin_stream
	-- if there is a breakin_stream playing already, close it
	-- when this breakin_stream is interrupted by others interrupted() is called.
	
	b.done(function ()
		-- breakin_stream playing finished
	end)
	
	b.close()
	-- close breakin_stream
	
	sw.stop_breakin()
	-- close current breakin_stream

## Stream copy

	c = pipe.copy(src, dst, 'rw')
	-- 'r' close src when finished
	-- 'w' close dst when finished
	
	c.close()
	-- finish copy immediatly
	
	c.done(function (reason) 
	-- reason == 
	--   'w' dst is closed
	--   'r' src is closed
	--   'c' close() is called
	end)
	
	c.pause()
	c.resume()
	-- pause/resume copying
	
	c.started(function ()
	-- called when got first buf from src 
	end)

# Pipe utils

	pipe.grep(src, 'word', function (line)
	end)
	-- read lines from src and grep the line contains 'word'
	
	pipe.readall(src, function (bigstr)
	end)
	-- read all lines from src
	

# Playlist

## Basic playlist

	list = {}
	
	list.fetch = function (done)
	-- get next song
	-- song == {title='Hey You', album='The Wall',
	--    artist='Pink Floyd', cover_url='', like=true/false}
	-- when fetching is done. call done(song)
	end

	list.cancel_fetch = function ()
	-- cancel fetching and done() wont't be called.
	end

## Url playlist

	list = playlist.urls {
		'audio1.mp3',
		'audio2.mp3',
		'audio3.mp3',
		loop=true,
	}
	
## Station playlist

for Douban / Pandora and other music stations:

 * fetch more than one song in a single HTTP request
 * need prefetch songs before playing
 * do login or channel change at any time. these operation will fetch new songs and discard songs already fetched.

each songs fetching operation is a `task`.

station should implement `prefetch_songs(task)`. it will be called at any time.

`restart_and_fetch_songs()` is provided and can be called when login / channel change. at any time.

 	douban.prefetch_songs = function (task)
 		fetch_songs_from_douban(function (songs)
 			task.done(songs)
 			-- songs == {
 			--   {title='song1', ...},
 			--   {title='song2', ...},
 			--   {title='song3', ...},
 			-- }
 		end)
 	end
 	
 	douban.login = function (user, pass)
 		local task = douban.restart_and_fetch_songs()
 		do_douban_login_and_fetch_new_songs(user, pass, function (songs)
 			task.done(songs)
 		end)
 	end

	list = playlist.station(douban)

## Player

	p = playlist.player()
	-- p is a stream
	pipe.copy(p, audio.out())
	
	p.setsrc(list1)
	p.setsrc(list2)
	-- set the current  playlist

	p.setsrc(nil)
	-- stop the playing
	
	p.next()
	-- play next song
	
	p.pause()
	p.resume()
	p.pause_resume()
	-- playing pause / resume / toggle pause,resume
	
	p.song
	-- current playing song == 
	--   {title='Hey You', album='The Wall',
	--    artist='Pink Floyd', cover_url='', like=true/false}
	-- nil if not playing
	
	p.pos
	-- current playing song's position in seconds
	-- nil if not playing
	
	p.stat	
	p.stat_changed(function (stat)
	-- stat == 
	--   'buffering'
	--   'playing'
	--   'paused'
	--   'fetching'
	--   'stopped'
	end)

