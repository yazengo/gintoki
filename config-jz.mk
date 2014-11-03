
sysroot = ../system/fs_compile

cobjs += audio_out_jzcodec.o inputdev.o alarm.o

luvmods += inputdev alarm

ldflags += -L$(sysroot)/lib
ldflags += -L$(sysroot)/lib/uv01022
ldflags += -llua

cflags += -I${sysroot}/include
cflags += -I${sysroot}/include/uv01022

CC = mipsel-linux-gcc
exe = server-mips

all: ${exe}

inst-files := tests *.lua testaudios bbcradio.json server-loop.sh

pack: ${exe}
	tar cf pack.tar ${exe} $(inst-files)

install: pack
	tar xvf pack.tar -C ../../../system/minifs/usr/app

