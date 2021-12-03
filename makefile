LIBS = -I/usr/include/libevdev-1.0/ -levdev -ljson-c
CFLAGS = -Wall -Ofast -std=c17
SRC = $(wildcard ./src/*.c)

rwug: $(SRC)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -f rwug