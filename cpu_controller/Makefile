CC=gcc
CFLAGS=-pedantic -std=c99 -Werror -Wextra -Wall -Wvla
DEFINES=-D_DEFAULT_C_SOURCE -D_GNU_SOURCE

BIN=cpu_controller

$(BIN): cpu_controller.c
	$(CC) -o ~/hsh/scripts/bin/$@ $(CFLAGS) $(DEFINES) $^
