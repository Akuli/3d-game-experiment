#include "place.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "../generated/filelist.h"
#include "misc.h"
#include "log.h"

/*
Small language for specifying places in assets/places/placename.txt files:
- 1x1 squares on xz plane with integer corner coordinates are built of parts like

	 --
	|  |
	 --

- any of the '--' or '|' walls may be replaced with spaces
- each line is padded with spaces to have same length
- must have these walls:
	- topmost wall (first line)
	- bottommost wall (last line)
	- left wall (first character of lines 1,3,5,... in 0-based indexing)
	- right wall (last character of those lines)
- NULL after last row
*/

static void reading_error(const char *path)
{
	log_printf_abort("error while reading '%s': %s", path, strerror(errno));
}

static void remove_trailing_newline(char *s)
{
	size_t len = strlen(s);
	if (len > 0 && s[len-1] == '\n')
		s[len-1] = '\0';
}

// make sure that s is big enough for len+1 bytes
static void add_trailing_spaces(char *s, size_t len)
{
	s[len] = '\0';
	memset(s + strlen(s), ' ', len - strlen(s));
	assert(strlen(s) == len);
}

static char *read_file_with_trailing_spaces_added(const char *path, int *linelen, int *nlines)
{
	FILE *f = fopen(path, "r");
	if (!f)
		log_printf_abort("opening '%s' failed: %s", path, strerror(errno));

	char buf[1024];
	*nlines = 0;
	*linelen = 0;
	while (fgets(buf, sizeof buf, f)) {
		++*nlines;
		remove_trailing_newline(buf);
		*linelen = max(*linelen, strlen(buf));
	}

	if (ferror(f))
		reading_error(path);
	if (*linelen == 0)
		log_printf_abort("file '%s' is empty or contains only newline characters", path);

	if (fseek(f, 0, SEEK_SET) < 0)
		log_printf_abort("seeking to beginning of file '%s' failed: %s", path, strerror(errno));

	char *res = calloc(*nlines, *linelen + 1);
	if (!res)
		log_printf_abort("not enough memory");

	for (char *ptr = res; ptr < res + (*linelen + 1)*(*nlines); ptr += *linelen + 1) {
		if (!fgets(buf, sizeof buf, f))
			reading_error(path);
		remove_trailing_newline(buf);
		assert(sizeof(buf) >= *linelen + 1);
		add_trailing_spaces(buf, *linelen);
		strcpy(ptr, buf);
	}

	fclose(f);
	return res;
}

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

static void init_place(struct Place *pl, const char *path)
{
	misc_basename_without_extension(path, pl->name, sizeof(pl->name));

	int linelen, nlines;
	char *fdata = read_file_with_trailing_spaces_added(path, &linelen, &nlines);

	/*
	 ---------> x
	|
	|
	|
	V
	z
	*/

	assert(linelen % 3 == 1);  // e.g. " -- " or "|  |", one more column means off by 3
	pl->xsize = linelen / 3;
	assert(nlines % 2 == 1);   // e.g. { " -- ", "|  |", " -- " }, one more row means off by 2
	pl->zsize = nlines/2;

	pl->nwalls = 0;
	for (int z = 0; z < pl->zsize; z++) {
		for (int x = 0; x < pl->xsize; x++) {
			//log_printf("Parsing lines %d,%d,%d starting at column %d in file '%s'", 2*z, 2*z+1, 2*z+2, 3*x, path);
			const char *toprow = fdata + (2*z    )*(linelen + 1) + 3*x;
			const char *midrow = fdata + (2*z + 1)*(linelen + 1) + 3*x;
			const char *botrow = fdata + (2*z + 2)*(linelen + 1) + 3*x;

			bool top, bottom, left, right;
			top = parse_horizontal_wall_string(toprow);
			parse_vertical_wall_string(midrow, &left, &right);
			bottom = parse_horizontal_wall_string(botrow);

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

	free(fdata);
	log_printf("created %d walls for place '%s'", pl->nwalls, pl->name);
}

const struct Place *place_list(void)
{
	static bool inited = false;
	static struct Place res[FILELIST_NPLACES];

	if (inited)
		return res;

	log_printf("start");
	for (int i = 0; i < FILELIST_NPLACES; i++)
		init_place(&res[i], filelist_places[i]);
	log_printf("end");
	inited = true;
	return res;
}
