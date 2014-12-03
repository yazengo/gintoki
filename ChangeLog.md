ChangeLog
----

# Pipe

* Add pipe_setopt() [141202][DONE]
* Set pipe attrbute by pipe_setopt(p, 'read_mode', 'normal') [141202][DONE]
* Remove 'b' attribute in pipe.copy() [141202][DONE]
* Always use pipebuf.len instead of PIPEBUF_SIZE [141202][DONE]
* Add pipe_setgc
* Rename PDIRECT_SRC/PDIRECT_SINK -> PDIRECT
* Replace every luv_newctx with pipe_new(type);
* Improve pcopy.buffering
  * enter buffering state when read blocks more than 1s
  * quit buffering state as soon as read starts

# Luv

* Change luv_gc_cb to void gc(void *_p);
* Find out the quickest way to do luv_newctx/luv_toctx. 

# Audio Nodes

* Add aeffect() API [141202][DONE]
  * aeffect_setopt(o, 'setvol', 33) 
  * aeffect_setopt(o, 'getvol')
  * aeffect_setopt(o, 'fadein', 300)
  * aeffect_setopt(o, 'fadeout', 300)
  * aeffect_setopt(o, 'cancel_read', 300)
* Remove setvol from amixer [141202][DONE]
* Add amixer_setopt instead of amixer.setopt [141202][DONE]
* Add aeffect test cases [141202][DONE]
* amixer do mix using first pipebuf as mixbuf instead of pipebuf_new [141202][DONE]

# SystemStat

* prof.lua: log important function calls times per second [DONE]
* statlog.lua: log system loadavg and cpu realtime [141202]

# Logging

* large string format to 'xxooxxoo ...(4123 bytes)'

# Sqlite

* Basic sqlite support

# HTTP Server

* mmap largefile read support
* ETAG support
* WebSocket support
* File uploading

# Shairport Support

* Multi-arch Makefile [141202][DONE]
* Test in Plat JZ [141202][DONE]

# Noise Generater

* WhiteNoise Generator [DONE]

# Radios

* BBC
* LocalMusic
* Test Pandora/Douban/BBC/LocalMusic
