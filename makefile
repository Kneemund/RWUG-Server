build:
	gcc -std=c99 -Wall -Ofast src/main.c -I/usr/include/libevdev-1.0/ -levdev -ljson-c -o rwug

clean:
	rm -f rwug
