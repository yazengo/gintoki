
cflags = -I.
ldflags = -g -luv -lm -lao

objs = utils.o main.o avconv.o strbuf.o hello.o
objs += audio_mixer.o audio_out.o audio_out_test.o
objs += upnp_device.o upnp_util.o  
objs += lua_cjson.o lua_cjson_fpconv.o
objs += ringbuf.o pcm.o

objs-x86 = $(subst .o,-x86.o,$(objs))
cflags-x86 = $(cflags) $(shell pkg-config --cflags lua5.2 libupnp libuv) 
ldflags-x86 = $(shell pkg-config --libs libupnp lua5.2) $(ldflags)

objs-mips = $(subst .o,-mips.o,$(objs))
cc-mips = mipsel-linux-gcc
sysroot-mips = ../muno-repo/app/source/system/fs_compile/
cflags-mips = $(cflags) -Ideps_mips/include/ -I$(sysroot-mips)/include/upnp
ldflags-mips = -L deps_mips/lib -L $(sysroot-mips)/lib  $(ldflags) -llua -pthread -lupnp -lthreadutil -lixml -lrt 
ldflags-mips += -lavcodec -lavutil -lavformat -lavdevice 

all: server-x86

%-mips.o: %.c
	$(cc-mips) $(cflags-mips) -c -o $@ $<

%-x86.o: %.c
	$(CC) $(cflags-x86) -c -o $@ $<

server-mips: $(objs-mips)
	$(cc-mips) -o $@ $(objs-mips) $(ldflags-mips) 

server-x86: $(objs-x86)
	$(CC) -o $@ $(objs-x86) $(ldflags-x86) 

clean:
	rm -rf *.o server-mips server-x86

