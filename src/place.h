#ifndef PLACE_H
#define PLACE_H

#include "max.h"
#include "wall.h"
#include "mathstuff.h"

struct Place {
	char path[1024];
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

// Result array changes only when places are added or removed
struct Place *place_list(int *nplaces);

// Returns index into *places
int place_copy(struct Place **places, int *nplaces, int srcidx);


#endif   // PLACE_H
