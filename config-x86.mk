
cobjs += audio_out_libao.o
ldflags += -lao -pthread -ldl -llua5.2

cflags += $(shell pkg-config --cflags lua5.2 ao)

