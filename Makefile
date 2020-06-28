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

SRC := $(wildcard src/*.c) generated/filelist.c
TESTS_SRC := $(wildcard tests/*.c)
HEADERS := $(wildcard src/*.h) generated/filelist.h
EXEDIR ?= .
COPIED_FILES := $(addprefix $(EXEDIR)/,$(notdir $(FILES_TO_COPY)))

# order matters, parallelizing make works best when slowly compiling things are first
OBJ := obj/stb_image.o $(SRC:%.c=obj/%.o)

# assetlist changes whenever output of 'ls -R assets' changes
UnusedVariableName := $(shell bash -c 'diff <(ls -R assets) assetlist || (ls -R assets > assetlist)')


all: $(EXEDIR)/game$(EXESUFFIX) test checkassets

# doesn't use .gitignore because it's sometimes handy to have files being
# ignored but not cleaned on rebuild
.PHONY: clean
clean:
	rm -rvf game generated build obj callgrind.out graph.* testrunner

generated/filelist.c generated/filelist.h: scripts/generate_filelist assetlist
	mkdir -p $(@D) && $< $(suffix $@) > $@

obj/%.o: %.c $(HEADERS)
	mkdir -p $(@D) && $(CC) -c -o $@ $< $(CFLAGS)

# "-x c" tells gcc to not treat the file as a header file
obj/stb_image.o: $(wildcard stb/*.h)
	mkdir -p $(@D) && \
	$(CC) -c -o $@ -x c \
	-DSTB_IMAGE_IMPLEMENTATION stb/stb_image.h $(VENDOR_CFLAGS)

$(EXEDIR)/game$(EXESUFFIX): $(OBJ) $(HEADERS) $(COPIED_FILES)
	mkdir -p $(@D) && $(CC) $(CFLAGS) $(OBJ) -o $@ $(LDFLAGS)

# this will need to be changed if i add more than one file of tests
$(EXEDIR)/testrunner$(EXESUFFIX): $(TESTS_SRC) $(SRC) $(HEADERS) $(COPIED_FILES)
	mkdir -p $(@D) && $(CC) -o $@ $(TESTS_SRC) $(CFLAGS) $(LDFLAGS)

.PHONY: test
test: $(EXEDIR)/testrunner$(EXESUFFIX)
	$(RUN) ./$<

.PHONY: check_assets_sources
checkassets:
	scripts/check_assets_sources

$(COPIED_FILES): $(FILES_TO_COPY)
	mkdir -p $(@D) && cp -r $(shell printf '%s\n' $^ | grep $(notdir $@)) $(EXEDIR)


# include checking stuff
#
#	https://github.com/include-what-you-use/include-what-you-use
#
# use autocomplete to figure out which clang/llvm version to use:
#
#	$ sudo apt install clang-<Tab><Tab>
#
# (here <Tab> denotes pressing the Tab key)
#
# add the resulting build/bin directory into your PATH by e.g. putting
# something like this to your ~/.bashrc:
#
#	PATH="$PATH:$HOME/iwyu/build/bin
#
# then open a new terminal to make sure you use the modified bashrc and:
#
#	$ make iwyu

# iwyu doesn't find stddef.h on my system by default (modify this if you
# use a different llvm/clang version with iwyu)
IWYUFLAGS += -I$(wildcard /usr/lib/llvm-8/lib/clang/8.*/include)

# please iwyu, don't warn me about stuff that c compilers warn about anyway...
IWYUFLAGS += -Wno-static-local-in-inline -Wno-absolute-value

IWYUFLAGS += -Xiwyu --mapping_file=iwyumappings.imp
IWYUFLAGS += -Xiwyu --no_fwd_decls

# iwyu/bla.c is not an actual file, just a name for makefile rule
# ||true because iwyu exits always with error status, lol
iwyu/%: %
	include-what-you-use $(IWYUFLAGS) $^ || true

.PHONY: iwyu
iwyu: $(addprefix iwyu/, $(wildcard src/*.c src/*.h))


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
