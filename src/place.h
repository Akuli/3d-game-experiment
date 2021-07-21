#ifndef PLACE_H
#define PLACE_H

#include <stdbool.h>
#include "max.h"
#include "wall.h"

struct PlaceCoords { int x, z; };

struct Place {
	char path[1024];
	bool custom;  // whether path starts with "custom_places"
	struct Wall walls[MAX_WALLS];
	int nwalls;
	int xsize, zsize;    // players and enemies must have 0 <= x <= xsize, 0 <= z <= zsize

	struct PlaceCoords playerlocs[2];
	struct PlaceCoords enemylocs[MAX_ENEMIES];
	int nenemylocs;
};

// Result array must be free()d
struct Place *place_list(int *nplaces);

// asserts that we are not hitting max number of walls
void place_addwall(struct Place *pl, int x, int z, enum WallDirection dir);

// Moves playerlocs, enemies and walls
// needs place_fix()
void place_movecontent(struct Place *pl, int dx, int dz);

// for resizing
void place_fix(struct Place *pl);

// for custom places only
void place_save(const struct Place *pl);

// May reallocate *places, returns index into it, saves copied place
int place_copy(struct Place **places, int *nplaces, int srcidx);

// Doesn't reallocate places
void place_delete(struct Place *places, int *nplaces, int srcidx);


#endif   // PLACE_H
