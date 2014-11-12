
cflags += -g -I. -Werror

ldflags += -g -lm -luv -lcurl

cobjs += main.o tests.o
cobjs += utils.o luv.o strbuf.o popen.o ringbuf.o timer.o os.o
cobjs += audio_mixer.o audio_out_test.o audio_in_avconv.o audio_in.o pcm.o
cobjs += cjson.o cjson_fpconv.o
cobjs += blowfish.o base64.o sha1.o
cobjs += airplay.o airplay_v2.o
cobjs += noise.o
cobjs += net.o curl.o http_parser.o zpnp.o itunes.o

luvmods += utils os audio_mixer popen curl zpnp blowfish base64 sha1 net airplay_v2 pcm timer noise

exe ?= server${objsuffix}
now = $(shell date +'%Y%m%d-%h%M')

config-mk = config$(if ${arch},-${arch},).mk
include ${config-mk}

all: ${exe}

hsrcs += $(wildcard *.h)
gitver = $(shell git rev-parse HEAD | sed 's/\(.......\).*/\1/')
luvinit = $(foreach m,${luvmods}, extern void luv_${m}_init(lua_State *L, uv_loop_t *); luv_${m}_init(L, loop);)

cflags-main += -DGITVER=\"${gitver}\" -DLUVINIT="${luvinit}" -DBUILDDATE=\"${now}\"

main.o: ${config-mk} Makefile

%${objsuffix}.o: %.c $(hsrcs)
	$(CC) $(cflags) $(cflags-$*) -c -o $@ $<

${exe}: ${cobjs}
	$(CC) -o $@ ${cobjs} ${ldflags}

clean:
	rm -rf *.o ${exe} config.h

