CC=gcc

kilo: kilo.c
	@ mkdir -p build
	@ $(CC) -std=c99 -Wall -Wextra -Wpedantic -o build/kilo kilo.c
