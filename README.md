# Gintoki 

## Overview

Gintoki is a multi-platform framework with a focus on fast and lightweight audio streaming applications.

 * Non-blocking I/O model. based on libuv and lua, designed for embeded  system. faster then node.js
 
 * Powerful audio pipeline: decoder, mixer, volume filter, fadein/fadeout filter, eq, audio output (libao), etc.

 * Playlist support. include Pandora and Douban. and your own radio station.

 * Airplay server (shairport)
 
 * HTTP client (libcurl), HTTP server (libuv+http-parser), TCP/UDP server/client, easily perform audio streaming over network.
 
 * Basic: urlopen, pexec, readdir, fopen, etc.

## Audio Pipelines

Decode audio file and play

	audio.pipe(fopen('a.mp3'), audio.decoder(), audio.out())
	# file stream -> audio decoder -> audio output
	
Fetch audio file from url and decode, play

	audio.pipe(pcurl('http://example/a.mp3'), audio.decoder(), audio.out())

Open any url

	audio.pipe(urlopen('a.mp3'), audio.decoder(), audio.out())

Simple airplay server in few lines

	a = airplay.server {
		name = 'MyAirplayServer',
		handler = function (r)
			audio.pipe(r, audio.out())
		end,
	}
	a.restart()
	a.rename('NewAirplayServer')

Play raw audio stream coming from TCP port 3128
	
	tcp.server {
		addr = ':3128',
		handler = function (r, w)
			audio.pipe(r, audio.out()).done(function ()
				w.close()
			end)
		end,
	}

Mix three different streams

	m = audio.mixer()
	audio.pipe(pcurl('http://example/main.mp3'), audio.decoder(), m.add())
	audio.pipe(audio.pipe(fopen('alert.mp3'), audio.decoder(), m.add())
	audio.pipe(fopen('noise.pcm'), m.add())
	audio.pipe(m, audio.out())
	
Switch streams dynamically
	
	ma = audio.pipe(fopen('music_a.mp3'), audio.decoder())
	mb = audio.pipe(fopen('music_b.mp3'), audio.decoder())
	
	p = audio.pipe(ma, audio.out())
	
	p.switchto(1, mb) # Switch to music a
	p.switchto(2, ma) # Switch to music b
	
Decode audio file into raw buffer and play
	
	ao = audio.out()
	raw = audio.buf()
	audio.pipe(fopen('a.mp3'), audio.decoder(), raw).done(function ()
		audio.pipe(raw.newsrc(), ao)
	end)

Control track volume in audio mixer
	
	m = audio.mixer()
	a = audio.pipe(...)
	b = audio.pipe(...)
	m.add(a)
	m.add(b)
	
	m.setvol(a, 10)
	m.setvol(b, 20)
	
	m.getvol(a)
	m.getvol(b)

Repeat play raw buffer. never ends.
	
	raw = audio.buf()
	audio.pipe(audio.decoder('a.mp3'), raw).done(function ()
		src = raw.newsrc{loop=true}
		audio.pipe(src, audio.out())
	end)
						
Set EQ
	
	eq = audio.eq()
	audio.pipe(fopen('a.mp3'), audio.decoder(), eq, audio.out())

	eq.on()
	eq.off()
	eq.set{10.0, 10.0, 1.0, 2.0, 3.0}
	
Url opener
	
	audio.pipe(urlopen('a.mp3'), audio.out())
	audio.pipe(urlopen('http://example/a.mp3'), audio.out())
	
Effetc chain

	audio.pipe(
		audio.decoder('a.mp3'),
		audio.eq{...}, 
		audio.fadein{time=300},
		audio.out()
	)

## Playlist

URL playlist

	p = radio.urls_list {
		"http://example/a.mp3", "/mnt/sdcard/b.mp3",
		loop=true,
	}
	audio.pipe(p.pipe(), audio.out())
	p.next()
	p.prev()

Play Douban/Pandora

	d = douban()
	audio.pipe(radio.new_station(d), audio.out())
	
	d.next()
	d.setopt{op='douban.login', username='', password=''}
	
	p = pandora()
	audio.pipe(radio.new_station(p), audio.out())
	
Custom your radio station
