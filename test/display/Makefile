VERSION := $(shell git describe --dirty --always)

CFLAGS += -g -I. -I../../src/
CFLAGS += -DSDL_DISPLAY
CFLAGS += -DVERSION=\"$(VERSION)\"
CFLAGS += $(shell pkg-config --cflags sdl)
LIBS += -lm
LIBS += $(shell pkg-config --libs sdl)

display_test: sdl_display.c main.c ../../src/fasttrigon.c ../../src/utils.c ../../src/menusystem.c aprs_fake.c lora_fake.c time_base_fake.c ../../src/wall_clock.c ../../src/display.c
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)
