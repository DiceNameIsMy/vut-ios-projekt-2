main:
	gcc -std=gnu99 -Wall -Wextra -Werror -pedantic src/main.c -o bin/main

test:
	gcc -std=gnu99 -Wall -Wextra -Werror -pedantic src/test.c -o bin/test

run: main
	./bin/main

test-run: test
	./bin/test