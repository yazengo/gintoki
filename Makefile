
cflags += -g -I. -Werror

ldflags += -g -lm -luv -lcurl

cobjs += main.o tests.o
cobjs += utils.o luv.o strbuf.o timer.o os.o immediate.o
cobjs += prof.o mem.o
cobjs += pipe.o uvwrite.o pstream.o pdirect.o 
cobjs += pexec.o pcopy.o pipebuf.o pfilebuf.o pfifo.o
cobjs += pstrsink.o
cobjs += asrc.o aout.o amixer.o pcm.o aeffect.o
cobjs += cjson.o cjson_fpconv.o
cobjs += blowfish.o base64.o sha1.o
cobjs += curl.o http_parser.o zpnp.o
cobjs += tcp.o

luvmods += pipebuf pdirect pipe pfilebuf
luvmods += utils os timer immediate
luvmods += prof
luvmods += blowfish base64 sha1 
luvmods += pexec pcopy pfifo
luvmods += pstrsink
luvmods += asrc aout amixer pcm aeffect
luvmods += curl zpnp 
luvmods += tcp

exe ?= gintoki
now = $(shell date +'%Y%m%d-%H%M')

arch ?= $(shell uname -s | tr A-Z a-z)

config-mk = config$(if ${arch},-${arch},).mk
include ${config-mk}

all: ${exe}

hsrcs += $(shell find -maxdepth 2 -name '*.h')
gitver = $(shell git rev-parse HEAD | sed 's/\(.......\).*/\1/')
luvinit = $(foreach m,${luvmods}, extern void luv_${m}_init(lua_State *L, uv_loop_t *); luv_${m}_init(L, loop);)

cflags-main += -DGITVER=\"${gitver}\" -DLUVINIT="${luvinit}" -DBUILDDATE=\"${now}\"

arch-cobjs := $(patsubst %.o,%_${arch}.o,${cobjs})

main.o: ${config-mk} Makefile

%_${arch}.o: %.c $(hsrcs)
	$(CC) $(cflags) $(cflags-$*) -c -o $@ $<

${exe}: ${arch-cobjs}
	$(CC) -o $@ ${arch-cobjs} ${ldflags}

clean:
	find -maxdepth 2 -name '*.o' | xargs rm -rf
	rm -rf ${exe}

