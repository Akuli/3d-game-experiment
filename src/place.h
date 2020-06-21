#ifndef PLACE_H
#define PLACE_H

#include "wall.h"

#define PLACE_MAX_WALLS 200

struct Place {
	const char *name;
	struct Wall walls[PLACE_MAX_WALLS];
	int nwalls;

	// players are allowed to be between x=0 and x=xsize
	int xsize, zsize;

	// rest of this struct is only for place.c
	const char *const *spec;
};

extern const int place_count;

/*
Returns a statically allocated array of places of length place_count (same
array every time). All walls are initialized.
*/
const struct Place *place_list(void);

struct Wall *place_load(const char *const *spec, int *nwalls);


#endif   // PLACE_H
