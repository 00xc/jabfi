CC=gcc
CFLAGS=-std=c99 -Wall -Wextra -pedantic -Ofast -ffast-math -fstack-protector-strong -fPIE -D_FORTIFY_SOURCE=2

jabfi: jabfi.c
	$(CC) $< $(CFLAGS) -o $@

tests: jabfi
	python3 tests/run_tests.py

clean: jabfi
	rm $<
