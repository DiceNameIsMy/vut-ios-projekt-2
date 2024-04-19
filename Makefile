CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic -lpthread src/journal.c src/sharing.c

default: build

build:
	$(CC) $(CFLAGS) src/main.c -o bin/main

run: build
	./bin/main

dbg: build
	{ echo -n '// '; cat src/main.c; } | tr "" "" > src/main-dbg.c
	$(CC) $(CFLAGS) -g -O0 src/main-dbg.c -o bin/main-dbg
	rm src/main-dbg.c

dbg-run: dbg
	./bin/main-dbg

test:
	$(CC) $(CFLAGS) -g -O0 src/test.c -o bin/test

test-run: test
	./bin/test