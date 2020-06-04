#include "place.h"
#include <assert.h>
#include "log.h"

/*
Small language for specifying places as strings:
- each character represents 1x1 square on xz plane with integer corner coordinates
- each character must be one of:
	- '|': wall on left side of square
	- '_': wall on bottom of square
	- 'L': wall on left and bottom of square
	- ' ': no walls
- each string must have same length, no more than PLACE_XSIZE_MAX
- the whole thing gets wrapped in invisible rectangular walls all around
	- for last string, wall on bottom is ignored
	- for first character of each string, wall on left is ignored
- no more strings than PLACE_ZSIZE_MAX
- NULL after last row
*/

static struct Place places[] = {
	{ "Foo", {0}, 0, 0, 0, (const char *[]){
		"_____|L_________",
		"_L___ _________L",
		"     ||         ",
	}},
};

const size_t place_count = sizeof(places)/sizeof(places[0]);
static bool places_inited = false;

static void add_wall(struct Place *pl, unsigned int x, unsigned int z, enum WallDirection dir)
{
	assert(pl->nwalls < PLACE_MAX_WALLS);

	struct Wall *w = &pl->walls[pl->nwalls++];
	w->startx = (int)x;
	w->startz = (int)z;
	w->dir = dir;
	wall_init(w);
}

static void init_place(struct Place *pl)
{
	assert(pl->name);
	assert(pl->spec[0]);
	pl->xsize = (unsigned int) strlen(pl->spec[0]);
	pl->zsize = 0;
	while (pl->spec[pl->zsize]) {
		assert(strlen(pl->spec[pl->zsize]) == pl->xsize);
		pl->zsize++;
	}
	assert(pl->zsize > 0);
	assert(pl->zsize > 0);
	assert(pl->zsize < PLACE_XSIZE_MAX);
	assert(pl->zsize < PLACE_ZSIZE_MAX);

	pl->nwalls = 0;

	/*
	 ---------> x
	|
	|
	|
	V
	z
	*/

	for (unsigned z = 0; z < pl->zsize; z++) {
		for (unsigned x = 0; x < pl->xsize; x++) {
			char c = pl->spec[z][x];
			if (c != '|' && c != '_' && c != 'L' && c != ' ')
				log_printf_abort("bad wall character '%c'", c);

			// walls for x=0 are added later
			if ((c == '|' || c == 'L') && (x != 0))
				add_wall(pl, x, z, WALL_DIR_ZY);

			// walls at PLACE_ZSIZE_MAX are added later
			// z+1 to make the wall go to bottom
			if ((c == '_' || c == 'L') && (z+1 != PLACE_ZSIZE_MAX))
				add_wall(pl, x, z+1, WALL_DIR_XY);

		}
	}

	// add surrounding walls
	for (unsigned x = 0; x < pl->xsize; x++) {
		add_wall(pl, x, 0, WALL_DIR_XY);
		add_wall(pl, x, pl->zsize, WALL_DIR_XY);
	}
	for (unsigned z = 0; z < pl->zsize; z++) {
		add_wall(pl, 0, z, WALL_DIR_ZY);
		add_wall(pl, pl->xsize, z, WALL_DIR_ZY);
	}
	printf("created %zu walls\n", pl->nwalls);
}

const struct Place *place_list(void)
{
	if (!places_inited) {
		for (size_t i = 0; i < place_count; i++)
			init_place(&places[i]);
	}
	return places;
}
