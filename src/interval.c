#include "interval.h"
#include <assert.h>
#include <stdbool.h>
#include "mathstuff.h"


static void remove_overlaps(struct Interval in, struct Interval *bot, struct Interval **top)
{
	for (struct Interval *p = bot; p < *top; p++) {
		int overlapstart = max(p->start, in.start);
		int overlapend = min(p->end, in.end);
		if (overlapstart >= overlapend)   // no overlap
			continue;

		bool leftpiece = (p->start < overlapstart);
		bool rightpiece = (p->end > overlapend);

		// remove overlap from *p
		if (leftpiece && rightpiece) {
			// it splits into two pieces
			*(*top)++ = (struct Interval){
				// left piece
				.start = p->start,
				.end = overlapstart,
				.id = p->id,
			};
			// right piece
			p->start = overlapend;
		} else if (leftpiece) {
			p->end = overlapstart;
		} else if (rightpiece) {
			p->start = overlapend;
		} else {
			// interval gets removed completely
			*p = *--*top;
		}
	}
}

size_t interval_non_overlapping(const struct Interval *in, size_t inlen, struct Interval *out)
{
	struct Interval *top = out;
	for (size_t i = 0; i < inlen; i++) {
		remove_overlaps(in[i], out, &top);
		*top++ = in[i];
	}

	assert(top >= out);
	return (size_t)(top - out);
}
