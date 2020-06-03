#ifndef PLACE_H
#define PLACE_H

#include "wall.h"

// real programmers (tm) make stuff fit their terminals, and write the (tm) sign in ascii
#define PLACE_XSIZE_MAX 80
#define PLACE_ZSIZE_MAX 24

/*
How many walls can there be?
+1 because ends included, and some more to avoid off-by-one (lol)
*/
#define PLACE_MAX_WALLS ((PLACE_XSIZE_MAX + 3)*(PLACE_ZSIZE_MAX + 3))

struct Place {
	const char *name;
	struct Wall walls[PLACE_MAX_WALLS];
	size_t nwalls;

	// players are allowed to be between x=0 and x=xsize
	unsigned int xsize, zsize;

	// rest of this struct is only for place.c
	const char *const *spec;
};

extern const size_t place_count;

// Returns a statically allocated array of places of length place_count, all walls initialized
const struct Place *place_list(void);

struct Wall *place_load(const char *const *spec, size_t *nwalls);


#endif   // PLACE_H
