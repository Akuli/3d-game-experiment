#include "region.h"
#include "log.h"

static bool exists_wall_between_points(const struct Place *pl, struct PlaceCoords p1, struct PlaceCoords p2)
{
	SDL_assert(
		(p1.x == p2.x && abs(p2.z - p1.z) == 1) ||
		(p1.z == p2.z && abs(p2.x - p1.x) == 1));

	for (int i = 0; i < pl->nwalls; i++) {
		if (
			(
				p1.x == p2.x
				&& pl->walls[i].dir == WALL_DIR_XY
				&& pl->walls[i].startx == p1.x
				&& pl->walls[i].startz == max(p1.z, p2.z)
			) || (
				p1.z == p2.z
				&& pl->walls[i].dir == WALL_DIR_ZY
				&& pl->walls[i].startx == max(p1.x, p2.x)
				&& pl->walls[i].startz == p1.z
			)
		) {
			return true;
		}
	}
	return false;
}

int region_size(const struct Place *pl, struct PlaceCoords start)
{
	char region[MAX_PLACE_SIZE][MAX_PLACE_SIZE] = {0};
	struct PlaceCoords *todo = malloc(sizeof todo[0]);
	SDL_assert(todo);
	todo[0] = start;
	int todolen = 1;
	int todoalloc = 1;

	while (todolen != 0) {
		struct PlaceCoords p = todo[--todolen];
		SDL_assert(0 <= p.x && p.x < pl->xsize);
		SDL_assert(0 <= p.z && p.z < pl->zsize);

		if (region[p.x][p.z])
			continue;
		region[p.x][p.z] = true;

		struct PlaceCoords neighbors[] = {
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
			if (!region[neighbors[i].x][neighbors[i].z] && !exists_wall_between_points(pl, p, neighbors[i]))
				todo[todolen++] = neighbors[i];
		}
	}
	free(todo);

	int count = 0;
	for (int x = 0; x < MAX_PLACE_SIZE; x++) {
		for (int z = 0; z < MAX_PLACE_SIZE; z++) {
			if (region[x][z])
				count++;
		}
	}
	return count;
}
