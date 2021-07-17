#ifndef PLACE_H
#define PLACE_H

#include "max.h"
#include "wall.h"
#include "mathstuff.h"

struct PlaceCoords { int x, z; };

struct Place {
	char path[1024];
	struct Wall walls[MAX_WALLS];
	int nwalls;
	int xsize, zsize;    // players and enemies must have 0 <= x <= xsize, 0 <= z <= zsize

	struct PlaceCoords enemyloc, playerlocs[2];   // initial places

	// locations for enemies with neverdie set to true, created when playing begins
	struct PlaceCoords neverdielocs[MAX_ENEMIES];
	int nneverdielocs;
};

// Result array must be free()d
struct Place *place_list(int *nplaces);


#endif   // PLACE_H
