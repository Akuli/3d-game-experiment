#ifndef PLACE_H
#define PLACE_H

#include "max.h"
#include "wall.h"
#include "mathstuff.h"

struct Place {
	char path[1024];
	bool custom;  // whether path starts with "custom_places"
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

// Result array must be free()d
struct Place *place_list(int *nplaces);

// asserts that we are not hitting max number of walls
void place_addwall(struct Place *pl, int x, int z, enum WallDirection dir);

// for custom places only
void place_save(const struct Place *pl);

/* Fix several inconsistencies:
- Delete duplicate walls and any walls outside the place
- Add walls around the place
- Move player and enemy locations inside the place
*/
void place_fix(struct Place *pl);

// May reallocate *places, returns index into it, saves copied place
int place_copy(struct Place **places, int *nplaces, int srcidx);

// Doesn't reallocate places
void place_delete(struct Place *places, int *nplaces, int srcidx);


#endif   // PLACE_H
