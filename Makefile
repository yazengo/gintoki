
cflags += -g -I. -Werror

ldflags += -g -lm -luv -lcurl

cobjs += main.o tests.o
cobjs += utils.o luv.o strbuf.o timer.o os.o immediate.o
cobjs += prof.o mem.o
cobjs += pipe.o uvwrite.o pstream.o pdirect.o pexec.o pcopy.o pipebuf.o
cobjs += strsink.o
cobjs += asrc.o aout.o amixer.o pcm.o
cobjs += cjson.o cjson_fpconv.o
cobjs += blowfish.o base64.o sha1.o
cobjs += curl.o http_parser.o zpnp.o

luvmods += pipebuf
luvmods += utils os timer immediate
luvmods += prof
luvmods += blowfish base64 sha1 
luvmods += pexec pcopy 
luvmods += strsink
luvmods += asrc aout amixer pcm
luvmods += curl zpnp 

exe ?= server${objsuffix}
now = $(shell date +'%Y%m%d-%H%M')

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

