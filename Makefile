#CFLAGS += -fsanitize=undefined -fsanitize=address
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Werror=incompatible-pointer-types -Werror=implicit-function-declaration -Werror=int-conversion -Werror=discarded-qualifiers
CFLAGS += -Werror=stack-usage=3000
CFLAGS += -g
CFLAGS += -O3
CFLAGS += -Istb
VENDOR_CFLAGS := $(CFLAGS:-W%=)   # no warnings from other people's code please
LDFLAGS += -lSDL2 -lSDL2_mixer
LDFLAGS += -lm

SRC := $(wildcard src/*.c)
TESTS_SRC := $(wildcard tests/*.c)
HEADERS := $(wildcard src/*.h)

# order matters, parallelizing make works best when slowly compiling things are first
OBJ := obj/stb_image.o obj/stb_image_resize.o $(SRC:src/%.c=obj/%.o)

all: game test

# doesn't use .gitignore because it's sometimes handy to have files being
# ignored but not cleaned on rebuild
.PHONY: clean
clean:
	rm -rvf game obj callgrind.out graph.*

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

game: $(OBJ) $(HEADERS)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

# this will need to be changed if i add more than one file of tests
testrunner: $(TESTS_SRC) $(SRC)
	$(CC) -o $@ $(TESTS_SRC) $(CFLAGS) $(LDFLAGS)

.PHONY: test
test: testrunner
	./$<
	python3 checkfuncs.py


# profiling stuff
#
#	$ python3 -m pip install gprof2dot
#	$ make graph.png

callgrind.out: game
	valgrind --tool=callgrind --callgrind-out-file=$@ ./$< --no-sound

graph.gv: callgrind.out
	gprof2dot $< --format=callgrind --output=$@

graph.png: graph.gv
	dot -Tpng $< -o $@
