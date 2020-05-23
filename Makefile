CFLAGS += -fsanitize=undefined #-fsanitize=address
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Werror=incompatible-pointer-types -Werror=implicit-function-declaration -Werror=int-conversion -Werror=discarded-qualifiers
CFLAGS += -Wno-unused-parameter -Wno-address
CFLAGS += -Werror=stack-usage=1024   # BUFSIZ is 8192 on my system
CFLAGS += -g
CFLAGS += -O3
LDFLAGS += -lSDL2

SRC := $(filter-out src/main.c, $(wildcard src/*.c))
OBJ := $(SRC:src/%.c=obj/%.o)
HEADERS := $(wildcard src/*.h src/objects/*.h)
TESTS_SRC := $(wildcard tests/*.c tests/objects/*.c)
TESTS_HEADERS := $(filter-out tests/runcalls.h, $(wildcard tests/*.h)) tests/runcalls.h

all: game

.PHONY: clean
clean:
	git clean -fXd

obj/%.o: src/%.c $(HEADERS)
	mkdir -p $(@D) && $(CC) -c -o $@ $< $(CFLAGS)

game: src/main.c $(OBJ) $(HEADERS)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LDFLAGS)
