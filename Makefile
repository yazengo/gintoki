
cc-mips = mipsel-linux-gcc

sysroot-mips = ../muno-repo/app/source/system/fs_compile/

cflags = -I.
cflags-mips = $(cflags) -Ideps_mips/include/ -I$(sysroot-mips)/include/upnp
cflags-x86 = $(cflags) $(shell pkg-config --cflags lua5.2 libupnp libuv) 

ldflags = -g -luv -lm -lavcodec -lavutil -lavformat -lavdevice
ldflags-x86 = $(shell pkg-config --libs libupnp lua5.2) $(ldflags)
ldflags-mips = -L deps_mips/lib -L $(sysroot-mips)/lib  $(ldflags) -llua -pthread -lupnp -lthreadutil -lixml -lrt 

objs = utils.o main.o avconv.o audio_out.o audio_out_test.o strbuf.o upnp_device.o upnp_util.o hello.o \
	lua_cjson.o lua_cjson_fpconv.o
objs-mips = $(subst .o,-mips.o,$(objs))
objs-x86 = $(subst .o,-x86.o,$(objs))

%-mips.o: %.c
	$(cc-mips) $(cflags-mips) -c -o $@ $<

%-x86.o: %.c
	$(CC) $(cflags-x86) -c -o $@ $<

all: server-mips server-x86

server-mips: $(objs-mips)
	$(cc-mips) -o $@ $(objs-mips) $(ldflags-mips) 

server-x86: $(objs-x86)
	$(CC) -o $@ $(objs-x86) $(ldflags-x86) 

clean:
	rm -rf *.o server-mips server-x86

