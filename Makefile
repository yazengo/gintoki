
cflags = -I.
ldflags = -g -luv -lm -lao

objs = utils.o main.o avconv.o strbuf.o tests.o
objs += audio_mixer.o audio_out.o audio_out_test.o
objs += upnp_device.o upnp_util.o  
objs += lua_cjson.o lua_cjson_fpconv.o
objs += ringbuf.o pcm.o
objs += lua_curl.o
objs += audio_in.o audio_in_avconv.o audio_in_airplay.o

objs-x86 = $(subst .o,-x86.o,$(objs))
cflags-x86 = $(cflags) $(shell pkg-config --cflags lua5.2 libupnp libuv) 
ldflags-x86 = $(shell pkg-config --libs libupnp lua5.2) $(ldflags)

objs-darwin = $(subst .o,-darwin.o,$(objs))
cflags-darwin += $(cflags)
cflags-darwin += -I/usr/local/Cellar/libupnp/1.6.19/include/upnp/
cflags-darwin += -I/usr/local/Cellar/lua52/5.2.3/include
ldflags-darwin += $(ldflags)
ldflags-darwin += -L/usr/local/Cellar/libupnp/1.6.19/lib
ldflags-darwin += -L/usr/local/Cellar/lua52/5.2.3/lib
ldflags-darwin += -L/usr/local/Cellar/libuv/0.10.21/lib
ldflags-darwin += -lupnp -llua -luv -lixml -lao

objs-mips = $(subst .o,-mips.o,$(objs))
cc-mips = mipsel-linux-gcc
sysroot-mips = ../muno-repo/app/source/system/fs_compile/
cflags-mips = $(cflags) -Ideps_mips/include/ -I$(sysroot-mips)/include/upnp
ldflags-mips = -L deps_mips/lib -L $(sysroot-mips)/lib  $(ldflags) -llua -pthread -lupnp -lthreadutil -lixml -lrt 
ldflags-mips += -lavcodec -lavutil -lavformat -lavdevice 

hfiles = $(wildcard *.h)

all: server-x86

%-x86.o: %.c
	$(CC) $(cflags-x86) -c -o $@ $<

%-darwin.o: %.c $(hfiles)
	$(CC) $(cflags-darwin) -c -o $@ $<

%-mips.o: %.c
	$(cc-mips) $(cflags-mips) -c -o $@ $<

server-x86: $(objs-x86)
	$(CC) -o $@ $(objs-x86) $(ldflags-x86) 

server-darwin: $(objs-darwin)
	$(CC) -o $@ $(objs-darwin) $(ldflags-darwin) 

server-mips: $(objs-mips)
	$(cc-mips) -o $@ $(objs-mips) $(ldflags-mips) 

darwin-install-deps:
	brew install lua52
	brew install libupnp
	brew install libuv
	brew install libao
	brew install libav

clean:
	rm -rf *.o server-mips server-x86 server-darwin

