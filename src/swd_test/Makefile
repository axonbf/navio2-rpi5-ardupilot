CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS =

all: swd_test

swd_test: swd_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f swd_test

.PHONY: all clean