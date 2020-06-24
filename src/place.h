#ifndef PLACE_H
#define PLACE_H

#include "max.h"
#include "wall.h"

struct Place {
	struct Wall walls[MAX_WALLS];
	int nwalls;
	int xsize, zsize;    // players and enemies must have 0 <= x <= xsize, 0 <= z <= zsize

	// y coords of these are set to 0
	Vec3 enemyloc, playerlocs[2];
};

/*
Returns a statically allocated array of places of length FILELIST_NPLACES (same
array every time). All walls are initialized.
*/
const struct Place *place_list(void);

struct Wall *place_load(const char *const *spec, int *nwalls);


#endif   // PLACE_H
