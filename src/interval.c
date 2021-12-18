#include "interval.h"
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "linalg.h"


// returns new value of top (easier than pointer to pointer)
static struct Interval *remove_overlaps(struct Interval in, struct Interval *bot, struct Interval *top)
{
	// looping backwards to make removing from list easier
	for (struct Interval *p = top - 1; p >= bot; p--) {
		int ostart = max(p->start, in.start);
		int oend = min(p->end, in.end);
		if (ostart >= oend)   // no overlap
			continue;

		bool leftpiece = (p->start < ostart);
		bool rightpiece = (p->end > oend);

		/*
		Remove overlap from *p, without messing up order. If we mess up order,
		then we might not always see transparent objects in the game, because the
		transparent objects get shown before something that was supposed to go
		under them.
		*/
		if (leftpiece && rightpiece) {
			// it splits into two pieces
			memmove(p+1, p, (top++ - p)*sizeof(p[0]));
			p[1].start = oend;    // right piece
			p[0].end = ostart;    // left piece
		} else if (leftpiece) {
			p->end = ostart;
		} else if (rightpiece) {
			p->start = oend;
		} else {
			// interval gets removed completely
			memmove(p, p+1, (--top - p)*sizeof(p[0]));
		}
	}
	return top;
}

int interval_non_overlapping(const struct Interval *in, int inlen, struct Interval *out)
{
	struct Interval *top = out;
	for (int i = 0; i < inlen; i++) {
		if (!in[i].allowoverlap)
			top = remove_overlaps(in[i], out, top);
		*top++ = in[i];
	}

	SDL_assert(out <= top && top <= out + INTERVAL_NON_OVERLAPPING_MAX(inlen));
	return (top - out);
}
