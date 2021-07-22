#include "region.h"
#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdlib.h>
#include "mathstuff.h"
#include "max.h"
#include "wall.h"

static bool exists_wall_between_points(const struct Map *map, struct MapCoords p1, struct MapCoords p2)
{
	SDL_assert(
		(p1.x == p2.x && abs(p2.z - p1.z) == 1) ||
		(p1.z == p2.z && abs(p2.x - p1.x) == 1));

	for (int i = 0; i < map->nwalls; i++) {
		if (
			(
				/*
				- p1 and p2 coordinates point towards top-left from actual location
				- Picture has p1.z < p2.z, but they could also be the other way
				- We are checking whether there is a wall at =========. Wall coordinates
				  of a wall like this specify the corner having smaller x value, hence
				  it's called startx

				 ---------> x
				|
				|
				|   (p1.x,p1.z)
				|
				|
				|                 p1
				|
				|
				|   (p2.x,p2.z)=========
				|
				|
				|                 p2
				V
				z
				*/
				p1.x == p2.x
				&& map->walls[i].dir == WALL_DIR_XY
				&& map->walls[i].startx == p1.x
				&& map->walls[i].startz == max(p1.z, p2.z)
			) || (
				p1.z == p2.z
				&& map->walls[i].dir == WALL_DIR_ZY
				&& map->walls[i].startx == max(p1.x, p2.x)
				&& map->walls[i].startz == p1.z
			)
		) {
			return true;
		}
	}
	return false;
}

int region_size(const struct Map *map, struct MapCoords start)
{
	char region[MAX_MAPSIZE][MAX_MAPSIZE] = {0};
	struct MapCoords *todo = malloc(sizeof todo[0]);
	SDL_assert(todo);
	todo[0] = start;
	int todolen = 1;
	int todoalloc = 1;

	while (todolen != 0) {
		struct MapCoords p = todo[--todolen];
		SDL_assert(0 <= p.x && p.x < map->xsize);
		SDL_assert(0 <= p.z && p.z < map->zsize);

		if (region[p.x][p.z])
			continue;
		region[p.x][p.z] = true;

		struct MapCoords neighbors[] = {
			{ p.x-1, p.z },
			{ p.x+1, p.z },
			{ p.x, p.z-1 },
			{ p.x, p.z+1 },
		};

		bool needrealloc = false;
		while (todoalloc < todolen + sizeof(neighbors)/sizeof(neighbors[0])) {
			todoalloc *= 2;
			needrealloc = true;
		}
		if (needrealloc) {
			todo = realloc(todo, sizeof(todo[0]) * todoalloc);
			SDL_assert(todo);
		}

		for (int i = 0; i < sizeof(neighbors)/sizeof(neighbors[0]); i++) {
			if (!region[neighbors[i].x][neighbors[i].z] && !exists_wall_between_points(map, p, neighbors[i]))
				todo[todolen++] = neighbors[i];
		}
	}
	free(todo);

	int count = 0;
	for (int x = 0; x < MAX_MAPSIZE; x++) {
		for (int z = 0; z < MAX_MAPSIZE; z++) {
			if (region[x][z])
				count++;
		}
	}
	return count;
}
