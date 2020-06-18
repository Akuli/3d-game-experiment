#!/usr/bin/env python3

# quick and dirty checker script

import functools
import glob
import os
import re
import textwrap


@functools.lru_cache()
def read(file):
    with open(file) as f:
        return f.read()


cfiles = glob.glob('src/*.c')

for cfile in cfiles:
    if cfile == 'src/main.c':
        continue

    hfile = cfile.rstrip('c') + 'h'

    declarations = re.findall(
        r'^(?:\w+ )+\w+\([^()]*?\);', read(hfile),
        flags=(re.DOTALL | re.MULTILINE))

    # look for functions declared in header files but not defined in .c files
    if cfile is not None:
        for declaration in declarations:
            if declaration.rstrip(';') not in read(cfile):
                print(("declaration in .h file without "
                       "corresponding definition in .c file:"), repr(declaration))

    # look for usages in other .c files
    for declaration in declarations:
        [funcname] = re.findall(r'(\w+)\(', declaration)
        if not any(
                re.search(fr'^[ \t].*\b{funcname}\(', read(other_cfile),
                          flags=re.MULTILINE) is not None
                # don't ignore usage in the same cfile as where it's defined
                for other_cfile in cfiles):
            print("function usage not found:", funcname)
