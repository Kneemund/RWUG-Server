build:
	gcc -std=c99 -Wall -g src/main.c -I/usr/include/libevdev-1.0/ -levdev -ljson-c -o wiiUGamepadServer

clean:
	rm -f wiiUGamepadServer
