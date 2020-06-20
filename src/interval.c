#include "interval.h"
#include <assert.h>
#include <stdbool.h>
#include "mathstuff.h"


bool interval_overlap(int start1, int end1, int start2, int end2)
{
	int ostart = max(start1, start2);
	int oend = min(end1, end2);

	/*
	Allow equality here, so that overlaps can happen with e.g. start1==end1. This
	helps with showing very thin walls on screen.
	*/
	return (ostart <= oend);
}

static void remove_overlaps(struct Interval in, struct Interval *bot, struct Interval **top)
{
	// looping backwards to make removing from list easier
	for (struct Interval *p = *top - 1; p >= bot; p--) {
		int ostart = max(p->start, in.start);
		int oend = min(p->end, in.end);
		if (ostart >= oend)   // no overlap
			continue;

		bool leftpiece = (p->start < ostart);
		bool rightpiece = (p->end > oend);

		// remove overlap from *p
		if (leftpiece && rightpiece) {
			// it splits into two pieces
			*(*top)++ = (struct Interval){
				// left piece
				.start = p->start,
				.end = ostart,
				.id = p->id,
			};
			// right piece
			p->start = oend;
		} else if (leftpiece) {
			p->end = ostart;
		} else if (rightpiece) {
			p->start = oend;
		} else {
			// interval gets removed completely
			*p = *--*top;
		}
	}
}

int interval_non_overlapping(const struct Interval *in, int inlen, struct Interval *out)
{
	struct Interval *top = out;
	for (int i = 0; i < inlen; i++) {
		remove_overlaps(in[i], out, &top);
		*top++ = in[i];
	}

	assert(top >= out);
	return (top - out);
}
