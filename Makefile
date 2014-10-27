
cflags = -g -I. -Werror
ldflags = -g -luv -lm

objs = utils.o main.o strbuf.o tests.o
objs += audio_mixer.o audio_out.o audio_out_test.o
objs += upnp_device.o upnp_util.o  
objs += lua_cjson.o lua_cjson_fpconv.o
objs += ringbuf.o pcm.o
objs += audio_in_avconv.o 
objs += blowfish.o 
objs += base64.o 
objs += sha1.o 
objs += audio_in.o
objs += airplay.o
objs += airplay_v2.o
objs += curl.o
objs += net.o
objs += http_parser.o
objs += zpnp.o
objs += popen.o

ldflags += -lcurl

cflags += -DVERSION=\"$(shell git rev-parse HEAD)\"

objs-x86 += $(subst .o,-x86.o,$(objs))
cflags-x86 += $(cflags)
cflags-x86 += $(shell pkg-config --cflags lua5.2 libupnp libuv) 
ldflags-x86 += $(ldflags)
ldflags-x86 += $(shell pkg-config --libs libupnp lua5.2)
ldflags-x86 += -lao

objs-darwin += $(subst .o,-darwin.o,$(objs))
cflags-darwin += $(cflags) -I/usr/local/include -I/usr/local/include/upnp
ldflags-darwin += $(ldflags) -L/usr/local/lib
ldflags-darwin += -lupnp -llua -lixml -lao

sysroot-mips = ../system/fs_compile/
cc-mips = mipsel-linux-gcc
objs-mips += $(subst .o,-mips.o,$(objs))
objs-mips += inputdev-mips.o
cflags-mips += $(cflags)
cflags-mips += -I$(sysroot-mips)/include
cflags-mips += -I$(sysroot-mips)/include/upnp
cflags-mips += -I$(sysroot-mips)/include/uv01022
cflags-mips += -DUSE_JZCODEC
cflags-mips += -DUSE_INPUTDEV
ldflags-mips += $(ldflags)
ldflags-mips += -L$(sysroot-mips)/lib
ldflags-mips += -L$(sysroot-mips)/lib/uv01022
ldflags-mips += -llua -pthread -lupnp -lthreadutil -lixml -lrt 

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

inst-files := tests *.lua testaudios upnpweb bbcradio.json

inst-mips: server-mips
	tar cf $@.tar server-mips $(inst-files)

inst-x86: server-x86
	tar cf $@.tar server-x86 $(inst-files)

cp-minifs-mips: inst-mips
	tar xvf inst-mips.tar -C ../../../system/minifs/usr/app

sumcode:
	wc -l audio*.[ch] utils.[ch] luv_curl.[ch] main.c ringbuf.[ch] inputdev.[ch] airplay*.[ch] popen.[ch]
	wc -l *.lua

clean:
	rm -rf *.o server-mips server-x86 server-darwin

