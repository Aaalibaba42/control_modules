CC=gcc
CFLAGS=-pedantic -std=c99 -Werror -Wextra -Wall -Wvla
CPPFLAGS=-D_DEFAULT_C_SOURCE -D_GNU_SOURCE

SRC=memory_controller.c
OBJ=$(SRC:.c=.o)
BIN=$(OBJ:.o=)

$(BIN): $(OBJ)

$(OBJ): $(SRC)

clean:
	$(RM) $(OBJ) $(BIN)
