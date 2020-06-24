#ifndef PLACE_H
#define PLACE_H

#include "max.h"
#include "wall.h"

struct Place {
	char name[50];
	struct Wall walls[MAX_WALLS];
	int nwalls;
	int xsize, zsize;    // players and enemies must have 0 <= x <= xsize, 0 <= z <= zsize
};

/*
Returns a statically allocated array of places of length FILELIST_NPLACES (same
array every time). All walls are initialized.
*/
const struct Place *place_list(void);

struct Wall *place_load(const char *const *spec, int *nwalls);


#endif   // PLACE_H
