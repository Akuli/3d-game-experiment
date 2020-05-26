#include "common.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

void nonfatal_error_internal(const char *file, long lineno, const char *whatfailed, const char *msg)
{
	// TODO: write to log somewhere?
	fprintf(stderr, "%s:%ld: ", file, lineno);
	if (msg)
		fprintf(stderr, "%s failed: %s\n", whatfailed, msg);
	else
		fprintf(stderr, "%s failed\n", whatfailed);
}

noreturn void fatal_error_internal(
	const char *file, long lineno, const char *whatfailed, const char *msg)
{
	nonfatal_error_internal(file, lineno, whatfailed, msg);
	abort();
}

int iclamp(int val, int min, int max)
{
	assert(min <= max);
	if (val < min)
		return min;
	if (val > max)
		return max;
	return val;
}
