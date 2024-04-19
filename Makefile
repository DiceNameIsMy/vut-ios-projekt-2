CC=gcc
CFLAGS=-std=gnu99 -Wall -Wextra -Werror -pedantic

default: build

build:
	$(CC) $(CFLAGS) src/main.c -o bin/main

run: build
	./bin/main

dbg:
	{ echo -n '// '; cat src/main.c; } | tr "" "" > src/main-dbg.c
	$(CC) $(CFLAGS) -g -O0 src/main-dbg.c -o bin/main-dbg
	rm src/main-dbg.c

dbg-run: dbg
	./bin/main-dbg

test:
	$(CC) $(CFLAGS) -g -00 src/test.c -o bin/test

test-run: test
	./bin/test