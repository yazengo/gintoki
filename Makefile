
cflags += -g -I. -Werror

ldflags += -g -lm -luv -llua -lcurl

cobjs += main.o utils.o strbuf.o popen.o ringbuf.o tests.o
cobjs += audio_mixer.o audio_out_test.o audio_in_avconv.o audio_in.o pcm.o
cobjs += cjson.o cjson_fpconv.o
cobjs += blowfish.o base64.o sha1.o
cobjs += airplay.o airplay_v2.o
cobjs += net.o curl.o http_parser.o zpnp.o

luvmods = utils audio_mixer popen curl zpnp blowfish base64 sha1 net airplay_v2 pcm

config-mk = config$(if ${arch},-${arch},).mk

exe ?= server${objsuffix}
include ${config-mk}

all: ${exe}

hsrcs += $(wildcard *.h)
gitver = $(shell git rev-parse HEAD | sed 's/\(.......\).*/\1/')

cflags-main = -DGITVER=\"${gitver}\"

config.h: ${config-mk} Makefile
	echo > $@
	echo '#define LUVMOD_INIT $(foreach m,$(luvmods),luv_$(m)_init(L, loop);)' >>$@
	echo >>$@
	echo $(foreach m,$(luvmods),'#include "$(m).h"\n') >>$@
	echo >>$@

%${objsuffix}.o: %.c $(hsrcs) config.h
	$(CC) $(cflags) $(cflags-$*) -c -o $@ $<

${exe}: ${cobjs}
	$(CC) -o $@ ${cobjs} ${ldflags}

clean:
	rm -rf *.o ${exe}

