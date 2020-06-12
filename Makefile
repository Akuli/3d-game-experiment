#CFLAGS += -fsanitize=undefined -fsanitize=address
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
CFLAGS += -Wconversion -Werror=int-conversion
CFLAGS += -Werror=incompatible-pointer-types
CFLAGS += -Werror=implicit-function-declaration
CFLAGS += -Werror=discarded-qualifiers
CFLAGS += -Werror=stack-usage=3000
CFLAGS += -g
CFLAGS += -O3
VENDOR_CFLAGS := $(CFLAGS:-W%=)   # no warnings from other people's code please
LDFLAGS += -lSDL2 -lSDL2_mixer
LDFLAGS += -lm

SRC := $(wildcard src/*.c)
TESTS_SRC := $(wildcard tests/*.c)
HEADERS := $(wildcard src/*.h)
EXEDIR ?= .
COPIED_DLLFILES := $(addprefix $(EXEDIR)/,$(notdir $(DLLFILES)))

# order matters, parallelizing make works best when slowly compiling things are first
OBJ := obj/stb_image.o $(SRC:src/%.c=obj/%.o)


all: $(EXEDIR)/game$(EXESUFFIX) test checkfuncs

# doesn't use .gitignore because it's sometimes handy to have files being
# ignored but not cleaned on rebuild
.PHONY: clean
clean:
	rm -rvf game build obj callgrind.out graph.* testrunner

obj/%.o: src/%.c $(HEADERS)
	mkdir -p $(@D) && $(CC) -c -o $@ $< $(CFLAGS)

# "-x c" tells gcc to not treat the file as a header file

obj/stb_image.o: $(wildcard stb/*.h)
	mkdir -p $(@D) && \
	$(CC) -c -o $@ -x c \
	-DSTB_IMAGE_IMPLEMENTATION stb/stb_image.h $(VENDOR_CFLAGS)

$(EXEDIR)/game$(EXESUFFIX): $(OBJ) $(HEADERS) $(COPIED_DLLFILES)
	mkdir -p $(@D) && $(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

# this will need to be changed if i add more than one file of tests
$(EXEDIR)/testrunner$(EXESUFFIX): $(TESTS_SRC) $(SRC) $(HEADERS) $(COPIED_DLLFILES)
	mkdir -p $(@D) && $(CC) -o $@ $(TESTS_SRC) $(CFLAGS) $(LDFLAGS)

.PHONY: test
test: $(EXEDIR)/testrunner$(EXESUFFIX)
	$(RUN) ./$<

.PHONY: checkfuncs
checkfuncs:
	python3 checkfuncs.py

$(COPIED_DLLFILES): $(DLLFILES)
	mkdir -p $(@D) && cp $(shell printf '%s\n' $^ | grep $(notdir $@)) $(EXEDIR)


# profiling stuff
#
#	$ sudo apt install valgrind
#	$ python3 -m pip install gprof2dot
#	$ make graph.png

callgrind.out: $(EXEDIR)/game$(EXESUFFIX)
	valgrind --tool=callgrind --callgrind-out-file=$@ ./$< --no-sound

graph.gv: callgrind.out
	gprof2dot $< --format=callgrind --output=$@

graph.png: graph.gv
	dot -Tpng $< -o $@
