#!/bin/bash

diff \
	<(git ls-files | grep -E '\.(png|wav)' | sort) \
	<(printf '%s\n' $(
		grep ': ' sources.txt | cut -d: -f1 | sort
	))

if [ $? -eq 0 ]; then
	echo "sources.txt is up to date"
	exit 0
fi

printf "\n\n********** Error: sources.txt seems to be outdated\n"
exit 1
