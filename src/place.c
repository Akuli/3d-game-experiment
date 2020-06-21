#include "place.h"
#include <assert.h>
#include "log.h"

/*
Small language for specifying places as strings:
- 1x1 squares on xz plane with integer corner coordinates are built of parts like

	 --
	|  |
	 --

- any of the '--' or '|' walls may be replaced with spaces
- each string must have same length
- the whole thing gets wrapped in a big rectangle of walls, so these walls are ignored:
	- topmost wall (first string)
	- bottommost wall (last string)
	- left wall (first character of strings 1,3,5,... in 0-based indexing)
	- right wall (last character of those strings)
- NULL after last row
*/

static struct Place places[] = {
	{ "Foo", {0}, 0, 0, 0, (const char *[]){
		" -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- ",
		"|                                                     |",
		"    --    -- --       --    -- --          --    --    ",
		"|     |     |  |     |        |     |  |  |     |     |",
		"    --                --                   --    --    ",
		"|     |     |  |        |     |     |  |  |     |     |",
		"    --    -- --       --             --                ",
		"|                                                     |",
		" -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- ",
		NULL,
	}},
};





const int place_count = sizeof(places)/sizeof(places[0]);
static bool places_inited = false;

static void add_wall(struct Place *pl, int x, int z, enum WallDirection dir)
{
	assert(pl->nwalls < PLACE_MAX_WALLS);

	struct Wall *w = &pl->walls[pl->nwalls++];
	w->startx = x;
	w->startz = z;
	w->dir = dir;
	wall_init(w);
}

// returns whether there is a wall
static bool parse_horizontal_wall_string(const char *part)
{
	// e.g. " -- "
	assert(part[0] == ' ');
	assert(part[1] == '-' || part[1] == ' ');
	assert(part[2] == part[1]);
	assert(part[3] == ' ');
	return (part[1] == '-');
}

static void parse_vertical_wall_string(const char *part, bool *leftwall, bool *rightwall)
{
	assert(part[0] == '|' || part[0] == ' ');
	assert(part[1] == ' ');
	assert(part[2] == ' ');
	assert(part[3] == '|' || part[3] == ' ');
	*leftwall = (part[0] == '|');
	*rightwall = (part[3] == '|');
}

static void init_place(struct Place *pl)
{
	assert(pl->name);
	assert(pl->spec[0]);

	size_t len = strlen(pl->spec[0]);
	assert(len % 3 == 1);  // e.g. " -- " or "|  |", one more column means off by 3
	pl->xsize = len / 3;
	assert(pl->xsize > 0);

	int n;
	for (n = 0; pl->spec[n]; n++)
		assert(strlen(pl->spec[pl->zsize]) == len);

	assert(n % 2 == 1);   // e.g. { " -- ", "|  |", " -- " }, one more row means off by 2
	pl->zsize = n/2;
	assert(pl->zsize > 0);

	pl->nwalls = 0;

	/*
	 ---------> x
	|
	|
	|
	V
	z
	*/

	for (int z = 0; z < pl->zsize; z++) {
		for (int x = 0; x < pl->xsize; x++) {
			bool top, bottom, left, right;
			top = parse_horizontal_wall_string(pl->spec[2*z] + 3*x);
			parse_vertical_wall_string(pl->spec[2*z + 1] + 3*x, &left, &right);
			bottom = parse_horizontal_wall_string(pl->spec[2*z + 2] + 3*x);

			// place must have surrounding left and top walls
			if (x == 0)
				assert(left);
			if (z == 0)
				assert(top);

			/*
			same for bottom and right, and when not last iteration of loop, we let
			another iteration of the loop to handle the wall
			*/
			if (x == pl->xsize - 1)
				assert(right);
			else
				right = false;

			if (z == pl->zsize - 1)
				assert(bottom);
			else
				bottom = false;

			if (top)    add_wall(pl, x,   z,   WALL_DIR_XY);
			if (bottom) add_wall(pl, x,   z+1, WALL_DIR_XY);
			if (left)   add_wall(pl, x,   z,   WALL_DIR_ZY);
			if (right)  add_wall(pl, x+1, z,   WALL_DIR_ZY);
		}
	}
	log_printf("created %d walls", pl->nwalls);
}

const struct Place *place_list(void)
{
	if (!places_inited) {
		for (int i = 0; i < place_count; i++)
			init_place(&places[i]);
		places_inited = true;
	}
	return places;
}
