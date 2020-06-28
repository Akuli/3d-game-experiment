#ifndef PLACE_H
#define PLACE_H

#include "max.h"
#include "wall.h"
#include "mathstuff.h"

struct Place {
	struct Wall walls[MAX_WALLS];
	int nwalls;
	int xsize, zsize;    // players and enemies must have 0 <= x <= xsize, 0 <= z <= zsize

	// y coords of locations are set to 0
	Vec3 enemyloc;       // initial position of enemies appearing with timeout
	Vec3 playerlocs[2];  // player initial positions

	// locations for enemies with neverdie set to true, created when playing begins
	Vec3 neverdielocs[MAX_ENEMIES];
	int nneverdielocs;
};

/*
Returns a statically allocated array of places of length FILELIST_NPLACES (same
array every time). All walls are initialized.
*/
const struct Place *place_list(void);

struct Wall *place_load(const char *const *spec, int *nwalls);


#endif   // PLACE_H
