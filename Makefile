#CFLAGS += -fsanitize=undefined -fsanitize=address
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
CFLAGS += -Wfloat-conversion -Wno-sign-compare -Werror=int-conversion
CFLAGS += -Werror=incompatible-pointer-types
CFLAGS += -Werror=implicit-function-declaration
CFLAGS += -Werror=discarded-qualifiers
CFLAGS += -Werror=stack-usage=50000
CFLAGS += -Wno-missing-field-initializers   # it's often handy to leave stuff zeroed
CFLAGS += -DSDL_ASSERT_LEVEL=2              # enable SDL_assert()
CFLAGS += -g
CFLAGS += -O3
VENDOR_CFLAGS := $(CFLAGS:-W%=)   # no warnings from other people's code please
LDFLAGS += -lSDL2 -lSDL2_mixer -lSDL2_ttf
LDFLAGS += -lm

SRC := $(wildcard src/*.c src/*/*.c)
TESTS_SRC := $(wildcard tests/test_*.c)
HEADERS := $(wildcard src/*.h src/*/*.h tests/*.h)
EXEDIR ?= .
COPIED_FILES := $(addprefix $(EXEDIR)/,$(notdir $(FILES_TO_COPY)))

# OBJ order matters, parallelizing make works best when slowly compiling things are first
OBJ := obj/stb_image.o $(SRC:%.c=obj/%.o)
TESTS_OBJ := $(TESTS_SRC:%.c=obj/%.o)

# each tests/test_bla.c file includes corresponding src/bla.c file, don't link both of them
TESTS_LINK_OBJ := $(TESTS_OBJ) $(filter-out obj/src/main.o $(TESTS_SRC:tests/test_%.c=obj/src/%.o), $(OBJ))


all: $(EXEDIR)/game$(EXESUFFIX) test checkassets

# doesn't use .gitignore because it's sometimes handy to have files being
# ignored but not cleaned on rebuild
.PHONY: clean
clean:
	rm -rvf game generated build obj callgrind.out graph.* testrunner

obj/%.o: %.c $(HEADERS)
	mkdir -p $(@D) && $(CC) -c -o $@ $< $(CFLAGS)

# "-x c" tells gcc to not treat the file as a header file
obj/stb_image.o: $(wildcard stb/*.h)
	mkdir -p $(@D) && \
	$(CC) -c -o $@ -x c \
	-DSTB_IMAGE_IMPLEMENTATION stb/stb_image.h $(VENDOR_CFLAGS)

$(EXEDIR)/game$(EXESUFFIX): $(OBJ) $(HEADERS) $(COPIED_FILES)
	mkdir -p $(@D) && $(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

# this is lol
generated/run_tests.h: $(TESTS_OBJ)
	mkdir -p $(@D) && strings $(TESTS_OBJ) | grep '^test_[a-z0-9_]*$$' | sort | uniq | awk '{ printf "RUN(%s);", $$1 }' > $@

$(EXEDIR)/testrunner$(EXESUFFIX): tests/main.c $(TESTS_LINK_OBJ) generated/run_tests.h
	mkdir -p $(@D) && $(CC) -o $@ $(filter-out generated/run_tests.h, $^) $(CFLAGS) $(LDFLAGS)

.PHONY: test
test: $(EXEDIR)/testrunner$(EXESUFFIX)
	$(RUN) ./$<

.PHONY: iwyu
iwyu:
	$(MAKE) -f Makefile.iwyu

.PHONY: check_assets_sources
checkassets:
	scripts/check_assets_sources

$(COPIED_FILES): $(FILES_TO_COPY)
	mkdir -p $(@D) && cp -r $(shell printf '%s\n' $^ | grep $(notdir $@)) $(EXEDIR)


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
