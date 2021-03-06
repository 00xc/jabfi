CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -Werror -pedantic -Ofast -fstack-protector-strong -fPIE -D_FORTIFY_SOURCE=2
DEBUG_FLAGS=-DDEBUG

jabfi: jabfi.c
	$(CC) $< $(CFLAGS) -o $@

debug: jabfi.c
	$(CC) $< $(DEBUG_FLAGS) $(CFLAGS) -o $@

tests: jabfi
	python3 tests/run_tests.py

clean:
	rm -f jabfi
	rm -f debug
