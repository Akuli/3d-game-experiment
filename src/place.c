#include "glob.h"
#include "place.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "max.h"
#include "misc.h"
#include "log.h"

/*
Small language for specifying places in assets/(default|custom)_places/placename.txt files:
- 1x1 squares on xz plane with integer corner coordinates are built of parts like

	 --
	|  |
	 --

- content of square doesn't have to be spaces like above, can also be:
	- 'p': initial player place (need two of these in the place)
	- 'e': initial place for enemies (need one of these in the place)
	- 'E': add an enemy that never dies. Use this for places otherwise unreacahble
	  by enemies
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
	SDL_assert(strlen(s) == len);
}

static char *read_file_with_trailing_spaces_added(const char *path, int *linelen, int *nlines)
{
	// TODO: windows can have issue with unicode in file names
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
		SDL_assert(sizeof(buf) >= *linelen + 1);
		add_trailing_spaces(buf, *linelen);
		strcpy(ptr, buf);
	}

	fclose(f);
	return res;
}

static void add_wall(struct Place *pl, int x, int z, enum WallDirection dir)
{
	SDL_assert(pl->nwalls < MAX_WALLS);

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
	SDL_assert(part[0] == ' ');
	SDL_assert(part[1] == '-' || part[1] == ' ');
	SDL_assert(part[2] == part[1]);
	SDL_assert(part[3] == ' ');
	return (part[1] == '-');
}

struct SquareParsingState {
	struct Place *place;
	Vec3 loc;
	Vec3 *playerlocptr;   // pointer into place->playerlocs
};

static void parse_square_content(char c, struct SquareParsingState *st)
{
	switch(c) {
	case ' ':
		break;
	case 'e':
		st->place->enemyloc = st->loc;
		break;
	case 'E':
		SDL_assert(st->place->nneverdielocs < MAX_ENEMIES);
		st->place->neverdielocs[st->place->nneverdielocs++] = st->loc;
		break;
	case 'p':
		SDL_assert(st->place->playerlocs <= st->playerlocptr && st->playerlocptr < st->place->playerlocs + 2);
		*st->playerlocptr++ = st->loc;
		break;
	default:
		log_printf_abort("expected ' ', 'e' or 'p', got '%c'", c);
	}
}

static void parse_vertical_wall_string(const char *part, bool *leftwall, bool *rightwall, struct SquareParsingState *st)
{
	SDL_assert(part[0] == '|' || part[0] == ' ');
	SDL_assert(part[3] == '|' || part[3] == ' ');
	*leftwall = (part[0] == '|');
	*rightwall = (part[3] == '|');
	parse_square_content(part[1], st);
	parse_square_content(part[2], st);
}

static void read_place_from_file(struct Place *pl, const char *path)
{
	log_printf("Reading place from '%s'...", path);
	if (strstr(path, "default_places") == path)
		pl->custom = false;
	else if (strstr(path, "custom_places") == path)
		pl->custom = true;
	else
		SDL_assert(0);

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

	SDL_assert(linelen % 3 == 1);  // e.g. " -- " or "|  |", one more column means off by 3
	pl->xsize = linelen / 3;
	SDL_assert(nlines % 2 == 1);   // e.g. { " -- ", "|  |", " -- " }, one more row means off by 2
	pl->zsize = nlines/2;

	pl->nwalls = 0;
	struct SquareParsingState st = { .place = pl, .playerlocptr = pl->playerlocs };
	for (int z = 0; z < pl->zsize; z++) {
		for (int x = 0; x < pl->xsize; x++) {
			st.loc = (Vec3){ x+0.5f, 0, z+0.5f };    // +0.5f gives center coords

			const char *toprow = fdata + (2*z    )*(linelen + 1) + 3*x;
			const char *midrow = fdata + (2*z + 1)*(linelen + 1) + 3*x;
			const char *botrow = fdata + (2*z + 2)*(linelen + 1) + 3*x;

			bool top, bottom, left, right;
			top = parse_horizontal_wall_string(toprow);
			parse_vertical_wall_string(midrow, &left, &right, &st);
			bottom = parse_horizontal_wall_string(botrow);

			// place must have surrounding left and top walls
			if (x == 0)
				SDL_assert(left);
			if (z == 0)
				SDL_assert(top);

			/*
			same for bottom and right, and when not last iteration of loop, we let
			another iteration of the loop to handle the wall
			*/
			if (x == pl->xsize - 1)
				SDL_assert(right);
			else
				right = false;

			if (z == pl->zsize - 1)
				SDL_assert(bottom);
			else
				bottom = false;

			if (top)    add_wall(pl, x,   z,   WALL_DIR_XY);
			if (bottom) add_wall(pl, x,   z+1, WALL_DIR_XY);
			if (left)   add_wall(pl, x,   z,   WALL_DIR_ZY);
			if (right)  add_wall(pl, x+1, z,   WALL_DIR_ZY);
		}
	}
	free(fdata);

	log_printf("    %s", pl->custom ? "custom" : "default");
	log_printf("    name '%s'", pl->name);
	log_printf("    size %dx%d", pl->xsize, pl->zsize);
	log_printf("    %d walls", pl->nwalls);
	log_printf("    %d enemies that never die", pl->nneverdielocs);
	log_printf("    enemies go to (%.2f, %.2f, %.2f)", pl->enemyloc.x, pl->enemyloc.y, pl->enemyloc.z);
	for (int i = 0; i < 2; i++)
		log_printf("    player %d goes to (%.2f, %.2f, %.2f)", i, pl->playerlocs[i].x, pl->playerlocs[i].y, pl->playerlocs[i].z);
}

const struct Place *place_list(int *nplaces)
{
	static int n = -1;
	static struct Place places[50];

	if (n == -1) {
		// called for first time
		glob_t gl;
		if (glob("default_places/*.txt", 0, NULL, &gl) != 0)
			log_printf_abort("can't find default places");

		// TODO: does this error on windows when custom_places not exists?
		int r = glob("custom_places/*.txt", GLOB_APPEND, NULL, &gl);
		if (r != 0 && r != GLOB_NOMATCH)
			log_printf_abort("error while globbing custom places");

		n = gl.gl_pathc;
		size_t i;
		for (i = 0; i < gl.gl_pathc && i < sizeof(places)/sizeof(places[0]); i++)
			read_place_from_file(&places[i], gl.gl_pathv[i]);
		for ( ; i < gl.gl_pathc; i++)
			log_printf("ignoring place because there's too many: %s", gl.gl_pathv[i]);

		globfree(&gl);
	}

	if(nplaces)
		*nplaces = n;
	return places;
}
