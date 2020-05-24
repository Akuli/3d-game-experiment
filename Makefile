#CFLAGS += -fsanitize=undefined #-fsanitize=address
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Werror=incompatible-pointer-types -Werror=implicit-function-declaration -Werror=int-conversion -Werror=discarded-qualifiers
CFLAGS += -Wno-unused-parameter -Wno-address
CFLAGS += -Werror=stack-usage=1024   # BUFSIZ is 8192 on my system
CFLAGS += -g
CFLAGS += -O0
CFLAGS += -Istb
VENDOR_CFLAGS := $(CFLAGS:-W%=)   # no warnings from other people's code please
LDFLAGS += -lSDL2 -lSDL2_gfx   # could also use:  pkg-config --libs SDL2_gfx
LDFLAGS += -lm

SRC := $(filter-out src/main.c, $(wildcard src/*.c))
OBJ := $(SRC:src/%.c=obj/%.o) obj/stb_image.o obj/stb_image_resize.o
HEADERS := $(wildcard src/*.h src/objects/*.h)
TESTS_SRC := $(wildcard tests/*.c tests/objects/*.c)
TESTS_HEADERS := $(filter-out tests/runcalls.h, $(wildcard tests/*.h)) tests/runcalls.h

all: game

.PHONY: clean
clean:
	git clean -fXd

obj/%.o: src/%.c $(HEADERS)
	mkdir -p $(@D) && $(CC) -c -o $@ $< $(CFLAGS)

# "-x c" tells gcc to not treat the file as a header file

obj/stb_image.o: $(wildcard stb/*.h)
	mkdir -p $(@D) && \
	$(CC) -c -o $@ -x c \
	-DSTB_IMAGE_IMPLEMENTATION stb/stb_image.h $(VENDOR_CFLAGS)

obj/stb_image_resize.o: $(wildcard stb/*.h)
	mkdir -p $(@D) && \
	$(CC) -c -o $@ -x c \
	-DSTB_IMAGE_RESIZE_IMPLEMENTATION stb/stb_image_resize.h $(VENDOR_CFLAGS)

game: src/main.c $(OBJ) $(HEADERS)
	$(CC) $(CFLAGS) $< $(OBJ) -o $@ $(LDFLAGS)
