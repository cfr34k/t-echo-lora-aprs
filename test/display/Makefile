VERSION := $(shell git describe --dirty --always)

CFLAGS += -g -I. -I../../src/
CFLAGS += -DSDL_DISPLAY
CFLAGS += -DVERSION=\"$(VERSION)\"
CFLAGS += $(shell pkg-config --cflags sdl)
LIBS += -lm
LIBS += $(shell pkg-config --libs sdl)

SRCS := sdl_display.c main.c ../../src/fasttrigon.c ../../src/utils.c \
	../../src/menusystem.c ../../src/aprs.c lora_fake.c time_base_fake.c \
	bme280_fake.c ../../src/wall_clock.c ../../src/display.c settings_fake.c

display_test: $(SRCS)
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)
