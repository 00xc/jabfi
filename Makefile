CC=gcc
CFLAGS=-std=c99 -Wall -O3 -ffast-math

jabfi: jabfi.c
	$(CC) $< $(CFLAGS) -o $@

tests: jabfi
	python3 tests/run_tests.py

clean: jabfi
	rm $<
