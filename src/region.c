#include "region.h"
#include "log.h"

static bool can_move(const struct Place *pl, struct PlaceCoords cur, int dx, int dz)
{
	SDL_assert(
		(dx == 0 && (dz == -1 || dz == 1)) ||
		(dz == 0 && (dx == -1 || dx == 1)));

	for (int i = 0; i < pl->nwalls; i++) {
		if (
			(
				dz != 0
				&& pl->walls[i].dir == WALL_DIR_XY
				&& pl->walls[i].startx == cur.x
				&& pl->walls[i].startz == min(cur.z, cur.z + dz)
			) || (
				dx != 0
				&& pl->walls[i].dir == WALL_DIR_ZY
				&& pl->walls[i].startx == min(cur.x, cur.x + dx)
				&& pl->walls[i].startz == cur.z
			)
		) {
			return false;
		}
	}
	return true;
}

int region_size(const struct Place *pl, struct PlaceCoords start)
{
	// tovisit is always a subset of the region
	bool region[MAX_PLACE_SIZE][MAX_PLACE_SIZE] = {0};
	struct PlaceCoords *todo = malloc(sizeof todo[0]);
	SDL_assert(todo);
	todo[0] = start;
	int todolen = 1;
	int todoalloc = 1;

	while (todolen != 0) {
		struct PlaceCoords p = todo[--todolen];
		region[p.x][p.z] = true;

		while (todoalloc < todolen + 4)
			todoalloc *= 2;
		log_printf("region_size(): realloc to %d elements", todoalloc);
		todo = realloc(todo, sizeof(todo[0]) * todoalloc);
		SDL_assert(todo);

		if (can_move(pl, p, -1, 0))
			todo[todolen++] = (struct PlaceCoords){ p.x-1, p.z };
		if (can_move(pl, p, 1, 0))
			todo[todolen++] = (struct PlaceCoords){ p.x+1, p.z };
		if (can_move(pl, p, 0, -1))
			todo[todolen++] = (struct PlaceCoords){ p.x, p.z-1 };
		if (can_move(pl, p, 0, 1))
			todo[todolen++] = (struct PlaceCoords){ p.x, p.z+1 };
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
