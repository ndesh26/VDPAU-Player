CC=gcc
CFLAGS=-lX11 -lvdpau

test: test.c
	$(CC) -o test test.c  $(CFLAGS)


