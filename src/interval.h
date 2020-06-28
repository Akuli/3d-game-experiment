#ifndef INTERVAL_H
#define INTERVAL_H

#include <stdbool.h>

// think of this as [start, end], i.e. real number interval with start and end included
struct Interval {
	int start;
	int end;
	int id;

	/*
	If this is true, then interval_non_overlapping() won't remove parts of
	intervals that are "under" this interval
	*/
	bool allowoverlap;
};

// do intervals [start1,end1] and [start2,end2] overlap?
bool interval_overlap(int start1, int end1, int start2, int end2);

/*
Given n intervals, we want an upper bound for how many non-overlapping intervals
they will be turned into.

Let f(n) denote the value that we want for this macro. Let's look at interval.c.
- If there is just one input interval, then the resulting array contains that
  interval unchanged.

	f(1) = 1

- If we add the n+1'th interval, then each of the n previous intervals can result
  in adding one more interval, and we also add the n+1'th interval:

	f(n+1) = f(n) + n + 1

f(n) can be solved from these equations.
*/
#define INTERVAL_NON_OVERLAPPING_MAX(n) ( ((n) + (n)*(n))/2 )

/*
The out array must have room for INTERVAL_NON_OVERLAPPING_MAX(inlen) elements.
Actual length of out array is returned.
*/
int interval_non_overlapping(const struct Interval *in, int inlen, struct Interval *out);


#endif   // INTERVAL_H
