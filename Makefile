
cflags = -g -I.
ldflags = -g -luv -lm

objs = utils.o main.o strbuf.o tests.o
objs += audio_mixer.o audio_out.o audio_out_test.o
objs += upnp_device.o upnp_util.o  
objs += lua_cjson.o lua_cjson_fpconv.o
objs += ringbuf.o pcm.o
objs += audio_in_avconv.o 
objs += blowfish.o 
objs += audio_in_airplay_proc.o

objs += luv_curl.o
cflags += -DUSE_CURL
ldflags += -lcurl

objs-x86 += $(subst .o,-x86.o,$(objs))
cflags-x86 += $(cflags) $(shell pkg-config --cflags lua5.2 libupnp libuv) 
ldflags-x86 += $(shell pkg-config --libs libupnp lua5.2) $(ldflags) -lao

objs-darwin += $(subst .o,-darwin.o,$(objs))
cflags-darwin += $(cflags)
cflags-darwin += -I../shairport/
cflags-darwin += -I/usr/local/Cellar/libupnp/1.6.19/include/upnp/
cflags-darwin += -I/usr/local/Cellar/lua52/5.2.3/include
ldflags-darwin += $(ldflags)
ldflags-darwin += -L/usr/local/Cellar/libupnp/1.6.19/lib
ldflags-darwin += -L/usr/local/Cellar/lua52/5.2.3/lib
ldflags-darwin += -L/usr/local/Cellar/libuv/0.10.21/lib
ldflags-darwin += -lupnp -llua -luv -lixml -lao

cc-mips = mipsel-linux-gcc
objs-mips += $(subst .o,-mips.o,$(objs))
objs-mips += inputdev-mips.o
sysroot-mips = ../system/fs_compile/
cflags-mips += $(cflags) -I$(sysroot-mips)/include/ -I$(sysroot-mips)/include/upnp
cflags-mips += -I../third/airplay-jz
cflags-mips += -DUSE_JZCODEC
cflags-mips += -DUSE_INPUTDEV
ldflags-mips += -L $(sysroot-mips)/lib  $(ldflags)
ldflags-mips += -llua -pthread -lupnp -lthreadutil -lixml -lrt 
ldflags-mips += -lavcodec -lavutil -lavformat -lavdevice 

hfiles = $(wildcard *.h)

all: server-x86

%-x86.o: %.c $(hfiles)
	$(CC) $(cflags-x86) -c -o $@ $<

%-darwin.o: %.c $(hfiles)
	$(CC) $(cflags-darwin) -c -o $@ $<

%-mips.o: %.c $(hfiles)
	$(cc-mips) $(cflags-mips) -c -o $@ $<

server-x86: $(objs-x86)
	$(CC) -o $@ $(objs-x86) $(ldflags-x86) 

server-darwin: $(objs-darwin)
	$(CC) -o $@ $(objs-darwin) $(ldflags-darwin) 

server-mips: $(objs-mips)
	$(cc-mips) -o $@ $(objs-mips) $(ldflags-mips) 

linux-install-deps:
	sudo apt-get install liblua52-dev libupnp-dev libuv-dev libao-dev libav-dev

darwin-install-deps:
	brew install lua52
	brew install libupnp
	brew install libuv
	brew install libao
	brew install libav

inst-mips: server-mips
	tar cvf $@.tar server-mips *.lua tests testaudios

cp-minifs-mips: inst-mips
	tar xvf inst-mips.tar -C minifs/usr/app

sumcode:
	wc -l audio_in*.[ch] audio_out*.[ch] utils.[ch] luv_curl.[ch] main.c ringbuf.[ch] inputdev.[ch]
	wc -l *.lua

clean:
	rm -rf *.o server-mips server-x86 server-darwin


