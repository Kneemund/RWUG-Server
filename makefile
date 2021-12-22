CFLAGS = -Wall -Ofast -std=c17
SRC = $(wildcard ./src/*.c)

rwug: $(SRC)
	gcc -o $@ $^ $(CFLAGS)

clean:
	rm -f rwug