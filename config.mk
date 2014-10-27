
sysroot = ../system/fs_compile

cobjs += audio_out_jzcodec.o inputdev.o

luvmods += inputdev

ldflags += -L$(sysroot)/lib
ldflags += -L$(sysroot)/lib/uv01022

cflags += -I${sysroot}/include
cflags += -I${sysroot}/include/uv01022

CC = mipsel-linux-gcc
exe = server-mips

all: ${exe}

inst-files := tests *.lua testaudios bbcradio.json server-loop.sh

inst-mips: server-mips
	tar cf $@.tar server-mips $(inst-files)

cp-minifs-mips: inst-mips
	tar xvf inst-mips.tar -C ../../../system/minifs/usr/app

