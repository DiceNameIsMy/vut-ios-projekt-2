default: build

build:
	gcc -std=gnu99 -Wall -Wextra -Werror -pedantic src/main.c -o bin/main

run: build
	./bin/main

dbg:
	{ echo -n '// '; cat src/main.c; } | tr "" "" > src/main-dbg.c
	gcc -std=gnu99 -g -O0 -Wall -Wextra -Werror -pedantic src/main-dbg.c -o bin/main-dbg
	rm src/main-dbg.c

dbg-run: dbg
	./bin/main-dbg

test:
	gcc -std=gnu99 -Wall -Wextra -Werror -pedantic src/test.c -o bin/test

test-run: test
	./bin/test