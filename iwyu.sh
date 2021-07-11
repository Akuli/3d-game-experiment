#!/bin/bash
set -e

FLAGS="$FLAGS -Xiwyu --mapping_file=iwyumappings.imp"
FLAGS="$FLAGS -Xiwyu --no_fwd_decls"

# Hide stuff that C compilers warn about anyway
FLAGS="$FLAGS -Wno-static-local-in-inline -Wno-absolute-value"

for file in src/*.c src/*.h; do
	# My glob thing is too dynamic or something, idk
	if [ $file == src/glob.c ] || [ $file == src/glob.h ]; then
		continue
	fi

	echo $file

	(iwyu $FLAGS $file || true) &> iwyu.out

	# Delete non-errors
	sed -i '/has correct #includes/d' iwyu.out

	# Fail if non-empty lines remain, those are error messages
	if grep -q . iwyu.out; then
		cat iwyu.out
		exit 1
	fi
done

