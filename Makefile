CC=gcc

kilo: kilo.c
	@ mkdir -p build
	@ $(CC) -std=c99 -Wall -Wextra -Wpedantic -o build/kilo kilo.c

dkilo: kilo.c
	@ mkdir -p build
	@ $(CC) -g -std=c99 -Wall -Wextra -Wpedantic -o build/dkilo kilo.c
