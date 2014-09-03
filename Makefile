
cflags = -g -I.
ldflags = -g -luv -lm -lao

USE_AIRPLAY = 1

objs = utils.o main.o strbuf.o tests.o
objs += audio_mixer.o audio_out.o audio_out_test.o
objs += upnp_device.o upnp_util.o  
objs += lua_cjson.o lua_cjson_fpconv.o
objs += ringbuf.o pcm.o
objs += audio_in_avconv.o 
#objs += strparser.o 

ifdef USE_CURL
objs += luv_curl.o
cflags += -DUSE_CURL
ldflags += -lcurl
endif

ifdef USE_AIRPLAY
objs += audio_in_airplay.o
cflags += -DUSE_AIRPLAY
endif

objs-x86 += $(subst .o,-x86.o,$(objs))
cflags-x86 += $(cflags) $(shell pkg-config --cflags lua5.2 libupnp libuv) 
cflags-x86 += -I../shairport/
ldflags-x86 += $(shell pkg-config --libs libupnp lua5.2) $(ldflags)
ldflags-x86 += -L../shairport
ldflags-x86 += -lshairport

objs-darwin += $(subst .o,-darwin.o,$(objs))
cflags-darwin += $(cflags)
cflags-darwin += -I../shairport/
cflags-darwin += -I/usr/local/Cellar/libupnp/1.6.19/include/upnp/
cflags-darwin += -I/usr/local/Cellar/lua52/5.2.3/include
ldflags-darwin += $(ldflags)
ldflags-darwin += -L/usr/local/Cellar/libupnp/1.6.19/lib
ldflags-darwin += -L/usr/local/Cellar/lua52/5.2.3/lib
ldflags-darwin += -L/usr/local/Cellar/libuv/0.10.21/lib
ldflags-darwin += -L../shairport/
ldflags-darwin += -lshairport
ldflags-darwin += -lupnp -llua -luv -lixml -lao

cc-mips = mipsel-linux-gcc
objs-mips += $(subst .o,-mips.o,$(objs))
objs-mips += inputdev-mips.o
sysroot-mips = ../muno-repo/app/source/system/fs_compile/
cflags-mips += $(cflags) -Ideps_mips/include/ -I$(sysroot-mips)/include/upnp
cflags-mips += -I../shairport-jz
cflags-mips += -DUSE_JZCODEC
cflags-mips += -DUSE_INPUTDEV
ldflags-mips += -L deps_mips/lib -L $(sysroot-mips)/lib  $(ldflags)
ldflags-mips += -L../shairport-jz
ldflags-mips += -lshairport
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

darwin-install-deps:
	brew install lua52
	brew install libupnp
	brew install libuv
	brew install libao
	brew install libav

inst-mips: server-mips
	tar cvf $@.tar server-mips *.lua tests

cp-minifs-mips: inst-mips
	tar xvf inst-mips.tar -C minifs/usr/app

clean:
	rm -rf *.o server-mips server-x86 server-darwin

