CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Werror -pedantic -O3 -funroll-loops -fstack-protector-strong -fPIE -D_FORTIFY_SOURCE=2
DEBUG_FLAGS = -DDEBUG -g

.PHONY: tests clean

jabfi: jabfi.c
	$(CC) $< $(CFLAGS) -o $@

debug: jabfi.c
	$(CC) $< $(DEBUG_FLAGS) $(CFLAGS) -o $@

tests: jabfi
	python3 tests/run_tests.py

clean:
	rm -f jabfi
	rm -f debug
