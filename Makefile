CC=gcc
CFLAGS=-g -pthread -Wall -fsanitize=address,undefined -std=c99

ww: ww.c
	$(CC) $(CFLAGS) -pthread -o $@ $^