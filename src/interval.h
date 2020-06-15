#ifndef INTERVAL_H
#define INTERVAL_H

#include <stdbool.h>
#include <stddef.h>

// think of this as [start, end], i.e. real number interval with start and end included
struct Interval {
	int start;
	int end;
	int id;
};

// do intervals [start1,end1] and [start2,end2] overlap?
bool interval_overlap(int start1, int end1, int start2, int end2);

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
int interval_non_overlapping(const struct Interval *in, int inlen, struct Interval *out);


#endif   // INTERVAL_H
