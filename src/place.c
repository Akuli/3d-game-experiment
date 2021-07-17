#include "glob.h"
#include "place.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "max.h"
#include "log.h"

#define COMPILE_TIME_STRLEN(s) (sizeof(s)-1)

/*
Small language for specifying places in assets/places/placename.txt files:
- 1x1 squares on xz plane with integer corner coordinates are built of parts like

	 --
	|  |
	 --

- content of square doesn't have to be spaces like above, can also be:
	- 'p': initial player place (need two of these in the place)
	- 'e': initial place for enemies (need one of these in the place)
	- 'E': add an enemy that never dies. Use this for places otherwise unreacahble
	  by enemies
- any of the '--' or '|' walls may be replaced with spaces, that means no wall
- each line is padded with spaces to have same length
- must have these walls:
	- wall at z=0 (first line)
	- wall at z=zsize (last line)
	- wall at x=0 (first character of lines 1,3,5,... in 0-based indexing)
	- wall at x=xsize (last character of those lines)
- NULL after last row
*/

#define MAX_LINE_LEN (COMPILE_TIME_STRLEN("|--")*MAX_PLACE_SIZE + COMPILE_TIME_STRLEN("|\n"))
#define MAX_LINE_COUNT (2*MAX_PLACE_SIZE + 1)

static const char *read_file_with_trailing_spaces_added(const char *path, int *nlines)
{
	FILE *f = fopen(path, "r");
	if (!f)
		log_printf_abort("opening \"%s\" failed: %s", path, strerror(errno));

	static char res[MAX_LINE_LEN*MAX_LINE_COUNT + 1];
	res[0] = '\0';

	char line[MAX_LINE_LEN + 1];
	int n = 0;
	while (fgets(line, sizeof line, f)) {
		SDL_assert(line[strlen(line)-1] == '\n');
		line[strlen(line)-1] = '\0';
		log_printf("%s", line);
		sprintf(res + strlen(res), "%-*s\n", (int)(MAX_LINE_LEN - strlen("\n")), line);

		SDL_assert(n < MAX_LINE_COUNT);
		n++;
	}
	if (ferror(f))
		log_printf_abort("can't read from \"%s\": %s", path, strerror(errno));
	fclose(f);

	*nlines = n;
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
	struct PlaceCoords loc;
	struct PlaceCoords *playerlocptr;   // pointer into place->playerlocs
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
		log_printf_abort("expected ' ', 'e', 'E' or 'p', got '%c'", c);
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

static void print_place_info(const struct Place *pl)
{
	log_printf("    path = %s", pl->path);
	log_printf("    size %dx%d", pl->xsize, pl->zsize);
	log_printf("    %d walls", pl->nwalls);
	log_printf("    %d enemies that never die", pl->nneverdielocs);
	log_printf("    enemies go to x=%d z=%d", pl->enemyloc.x, pl->enemyloc.z);
	for (int i = 0; i < 2; i++)
		log_printf("    player %d goes to (%d, %d)", i, pl->playerlocs[i].x, pl->playerlocs[i].z);
}

static const char *next_line(const char *s)
{
	char *nl = strchr(s, '\n');
	SDL_assert(nl);
	return nl+1;
}

static void read_place_from_file(struct Place *pl, const char *path)
{
	log_printf("Reading place from '%s'...", path);
	SDL_assert(strlen(path) < sizeof pl->path);
	strcpy(pl->path, path);

	int nlines;
	const char *fdata = read_file_with_trailing_spaces_added(path, &nlines);

	/*
	 -----> x
	|
	|
	V
	z
	*/
	SDL_assert(nlines % 2 == 1 && nlines >= 3);   // e.g. { " -- ", "|  |", " -- " }, one more row means off by 2
	pl->zsize = nlines/2;

	const char *secondline = next_line(fdata);
	int linelen = next_line(secondline) - secondline;
	while (secondline[linelen-1] != '|') {
		linelen--;
		SDL_assert(linelen > 0);
	}
	SDL_assert(linelen % 3 == 1 && linelen >= 4);  // e.g. "|  |", one more column means off by 3
	pl->xsize = linelen / 3;

	pl->nwalls = 0;
	struct SquareParsingState st = { .place = pl, .playerlocptr = pl->playerlocs };
	for (int z = 0; z < pl->zsize; z++) {
		const char *line1 = fdata;
		const char *line2 = next_line(line1);
		const char *line3 = next_line(line2);
		fdata = line3;

		for (int x = 0; x < pl->xsize; (x++, line1 += 3, line2 += 3, line3 += 3)) {
			st.loc = (struct PlaceCoords){x,z};

			bool top, bottom, left, right;
			top = parse_horizontal_wall_string(line1);
			parse_vertical_wall_string(line2, &left, &right, &st);
			bottom = parse_horizontal_wall_string(line3);

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

	print_place_info(pl);
}

struct Place *place_list(int *nplaces)
{
	glob_t gl;
	if (glob("assets/places/*.txt", 0, NULL, &gl) != 0)
		log_printf_abort("default places not found");

	struct Place *places = malloc(gl.gl_pathc * sizeof places[0]);
	if (!places)
		log_printf_abort("not enough memory for %d places", *nplaces);

	for (int i = 0; i < gl.gl_pathc; i++)
		read_place_from_file(&places[i], gl.gl_pathv[i]);
	globfree(&gl);

	*nplaces = gl.gl_pathc;
	return places;
}
