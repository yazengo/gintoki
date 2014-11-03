
cobjs += audio_out_libao.o
ldflags += -lao -pthread -ldl

cflags += $(shell pkg-config --cflags lua5.2 ao)

