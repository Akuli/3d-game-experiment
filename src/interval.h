#ifndef INTERVAL_H
#define INTERVAL_H

#include <stddef.h>

struct Interval {
	// think of this as [start, end], i.e. real number interval with start and end included
	int start;
	int end;
	int id;
};

/*
Given n intervals, we want an upper bound for how many non-overlapping intervals
they will be turned into. The definition below works because interval.c adds at
most 2 new intervals for each input interval.
*/
#define INTERVAL_NON_OVERLAPPING_MAX(n) (2*(n))

/*
The out array must have room for INTERVAL_NON_OVERLAPPING_MAX(inlen) elements.
Actual length of out array is returned.
*/
size_t interval_non_overlapping(const struct Interval *in, size_t inlen, struct Interval *out);


#endif   // INTERVAL_H
