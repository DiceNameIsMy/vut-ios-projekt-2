CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic -lpthread
CFLAGS += src/journal.c src/sharing.c src/ski_resort.c

default: build

build:
	$(CC) $(CFLAGS) src/main.c -o bin/main

run: build
	./bin/main

dbg:
	$(CC) $(CFLAGS) -g -O0 -DDEBUG src/main.c -o bin/main-dbg

dbg-run: dbg
	./bin/main-dbg

test:
	$(CC) $(CFLAGS) -g -O0 -DDEBUG src/test.c -o bin/test

test-run: test
	./bin/test