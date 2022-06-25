CFLAGS += -g -I. -I../../src/
CFLAGS += -DSDL_DISPLAY
CFLAGS += $(shell pkg-config --cflags sdl)
LIBS += -lm
LIBS += $(shell pkg-config --libs sdl)

display_test: sdl_display.c main.c ../../src/fasttrigon.c ../../src/utils.c ../../src/menusystem.c
	$(CC) -o $@ $(CFLAGS) $(LDFLAGS) $^ $(LIBS)
