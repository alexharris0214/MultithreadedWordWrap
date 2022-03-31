CC=gcc
CFLAGS=-g -Wall -fsanitize=address,undefined -std=c99

ww: ww.c
	$(CC) $(CFLAGS) -o $@ $^