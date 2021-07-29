#ifndef MAP_H
#define MAP_H

#include <stdbool.h>
#include "max.h"
#include "wall.h"

struct MapCoords { int x, z; };

struct Map {
	char path[1024];
	char name[25];   // Must fit in map chooser, map delete dialog and map name entry
	double sortkey;
	bool custom;  // whether path starts with "custom_maps"
	struct Wall walls[MAX_WALLS];
	int nwalls;
	int xsize, zsize;    // players and enemies must have 0 <= x <= xsize, 0 <= z <= zsize

	struct MapCoords playerlocs[2];
	struct MapCoords enemylocs[MAX_ENEMIES];
	int nenemylocs;
};

// Result array must be free()d
struct Map *map_list(int *nmaps);

// asserts that we are not hitting max number of walls
void map_addwall(struct Map *map, int x, int z, enum WallDirection dir);

// find a location where there is not enemy or player
// new location is usually near hint, but could be far if map e.g. contains lots of enemies
struct MapCoords map_findempty(const struct Map *map, struct MapCoords hint);

// Moves playerlocs, enemies and walls
// needs map_fix()
void map_movecontent(struct Map *map, int dx, int dz);

// for resizing
void map_fix(struct Map *map);

// for custom maps only
void map_save(const struct Map *map);

// reallocates *maps, returns index into it, saves copied map
int map_copy(struct Map **maps, int *nmaps, int srcidx);

// Doesn't reallocate maps
void map_delete(struct Map *maps, int *nmaps, int srcidx);


#endif   // MAP_H
