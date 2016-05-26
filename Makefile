CC=gcc
CFLAGS=-lX11 -lvdpau

test: test.o
	$(CC) -o test test.o  $(CFLAGS)


