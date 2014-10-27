
sysroot = ../system/fs_compile

cobjs += audio_out_jzcodec.o

ldflags += -L$(sysroot)/lib
ldflags += -L$(sysroot)/lib/uv01022

cflags += -I${sysroot}/include
cflags += -I${sysroot}/include/uv01022

CC = mipsel-linux-gcc
exe = server-mips

all: ${exe}

inst-files := tests *.lua testaudios upnpweb bbcradio.json server-loop.sh

cp-minifs-mips: inst-mips
	tar xvf inst-mips.tar -C ../../../system/minifs/usr/app

