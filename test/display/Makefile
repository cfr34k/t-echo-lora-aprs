CFLAGS += -g -I../../src/
CFLAGS += $(shell pkg-config --cflags sdl)
LIBS += -lm
LIBS += $(shell pkg-config --libs sdl)

display_test: sdl_display.c main.c ../../src/fasttrigon.c ../../src/utils.c
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)
