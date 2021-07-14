#ifndef PLACE_H
#define PLACE_H

#include "max.h"
#include "wall.h"
#include "mathstuff.h"

struct Place {
	char name[128];
	bool custom;
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

// Result is same array every time, cleaned when program exits
struct Place *place_list(int *nplaces);

struct Wall *place_load(const char *const *spec, int *nwalls);


#endif   // PLACE_H
