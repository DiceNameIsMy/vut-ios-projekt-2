main:
	gcc -std=gnu99 -Wall -Wextra -Werror -pedantic src/main.c -o bin/main

test:
	gcc -std=gnu99 -Wall -Wextra -Werror -pedantic src/test.c -o bin/test

run: main
	./bin/main

main-dbg:
	{ echo -n '// '; cat src/main.c; } | tr "" "" > src/main-dbg.c
	gcc -std=gnu99 -Wall -Wextra -Werror -pedantic src/main-dbg.c -o bin/main-dbg
	rm src/main-dbg.c

run-dbg: main-dbg
	./bin/main-dbg

test-run: test
	./bin/test