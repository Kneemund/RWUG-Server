CFLAGS_PIPEWIRE = $(shell pkg-config --cflags --libs libpipewire-0.3)
CFLAGS_GIO = $(shell  pkg-config --cflags --libs gio-2.0 gio-unix-2.0)
CFLAGS_LIBEVDEV = $(shell  pkg-config --cflags --libs libevdev)
CFLAGS_JSON-C = $(shell  pkg-config --cflags --libs json-c)
SRC = $(wildcard ./src/*.c)

rwug: $(SRC)
	gcc -o $@ $^ $(CFLAGS_LIBEVDEV) $(CFLAGS_JSON-C) $(CFLAGS_GIO) $(CFLAGS_PIPEWIRE)

clean:
	rm -f rwug