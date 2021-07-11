#!/bin/bash
set -e

for file in src/*.c src/*.h; do
	# My glob thing is too dynamic or something, idk
	if [ $file == src/glob.c ] || [ $file == src/glob.h ]; then
		continue
	fi

	echo $file

	# -W options to hide stuff that c compilers warn about anyway
	command="iwyu \
		-Wno-static-local-in-inline -Wno-absolute-value \
		-Xiwyu --mapping_file=iwyumappings.imp \
		-Xiwyu --no_fwd_decls \
		$file"
	($command || true) &> iwyu.out

	# Delete non-errors
	sed -i '/has correct #includes/d' iwyu.out

	# Fail if non-empty lines remain, those are error messages
	if grep -q . iwyu.out; then
		cat iwyu.out
		exit 1
	fi
done

