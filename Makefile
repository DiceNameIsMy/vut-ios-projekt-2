CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic -lpthread -lrt
CFLAGS += src/journal.c src/sharing.c src/ski_resort.c src/simulation.c

default: release

release:
	$(CC) $(CFLAGS) src/main.c -o proj2

build:
	$(CC) $(CFLAGS) src/main.c -o bin/main

run: build
	./bin/main

dbg:
	$(CC) $(CFLAGS) -ggdb3 -O0 -DDEBUG src/main.c -o bin/main-dbg

dbg-run: dbg
	./bin/main-dbg

zip:
	zip -r proj2.zip Makefile src include
