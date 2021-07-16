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
#include <assert.h>

#define COMPILE_TIME_STRLEN(s) (sizeof(s)-1)

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

	char buf[COMPILE_TIME_STRLEN("|--")*MAX_PLACE_SIZE + COMPILE_TIME_STRLEN("|\n") + sizeof("")];
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

static void print_place_info(const struct Place *pl)
{
	log_printf("    path = %s", pl->path);
	log_printf("    custom = %s", pl->custom ? "true" : "false");
	log_printf("    size %dx%d", pl->xsize, pl->zsize);
	log_printf("    %d walls", pl->nwalls);
	log_printf("    %d enemies that never die", pl->nneverdielocs);
	log_printf("    enemies go to (%.2f, %.2f, %.2f)", pl->enemyloc.x, pl->enemyloc.y, pl->enemyloc.z);
	for (int i = 0; i < 2; i++)
		log_printf("    player %d goes to (%.2f, %.2f, %.2f)", i, pl->playerlocs[i].x, pl->playerlocs[i].y, pl->playerlocs[i].z);
}

static void read_place_from_file(struct Place *pl, const char *path, bool custom)
{
	log_printf("Reading place from '%s'...", path);
	snprintf(pl->path, sizeof pl->path, "%s", path);
	pl->custom = custom;

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

	print_place_info(pl);
	free(fdata);
}

struct Place *place_list(int *nplaces)
{
	glob_t gl;
	if (glob("assets/default_places/*.txt", 0, NULL, &gl) != 0)
		log_printf_abort("default places not found");

	int ndefault = gl.gl_pathc;
	int r = glob("custom_places/custom-*.txt", GLOB_APPEND, NULL, &gl);
	if (r != 0 && r != GLOB_NOMATCH)
		log_printf_abort("error while globbing custom places");

	struct Place *places = malloc(gl.gl_pathc * sizeof places[0]);
	if (!places)
		log_printf_abort("not enough memory for %d places", *nplaces);

	for (int i = 0; i < gl.gl_pathc; i++)
		read_place_from_file(&places[i], gl.gl_pathv[i], i >= ndefault);
	globfree(&gl);

	*nplaces = gl.gl_pathc;
	return places;
}

static void set_char(char *data, int linesz, int x, int z, char c, int offset)
{
	int idx = (2*z + (c != '-'))*linesz + strlen("|--")*x + offset;
	data[idx] = c;
}

void place_save(const struct Place *pl)
{
	SDL_assert(pl->custom);
	int linesz = strlen("|--")*pl->xsize + strlen("|\n");
	int linecount = 2*pl->zsize + 1;

	char *data = malloc(linesz*linecount);
	if (!data)
		log_printf_abort("not enough memory");

	memset(data, ' ', linesz*linecount);
	for (int lineno = 0; lineno < linecount; lineno++)
		data[lineno*linesz + (linesz-1)] = '\n';

	for (const struct Wall *w = pl->walls; w < &pl->walls[pl->nwalls]; w++) {
		switch(w->dir) {
		case WALL_DIR_XY:
			set_char(data, linesz, w->startx, w->startz, '-', 1);
			set_char(data, linesz, w->startx, w->startz, '-', 2);
			break;
		case WALL_DIR_ZY:
			set_char(data, linesz, w->startx, w->startz, '|', 0);
			break;
		}
	}

	set_char(data, linesz, (int)pl->enemyloc.x, (int)pl->enemyloc.z, 'e', 1);
	for (int i = 0; i < 2; i++)
		set_char(data, linesz, (int)pl->playerlocs[i].x, (int)pl->playerlocs[i].z, 'p', 1);

	misc_mkdir("custom_places");  // pl->path is like "custom_places/custom-00006.txt"
	FILE *f = fopen(pl->path, "w");
	if (!f)
		log_printf_abort("opening \"%s\" failed: %s", pl->path, strerror(errno));
	if (fwrite(data, 1, linesz*linecount, f) != linesz*linecount)
		log_printf_abort("writing to \"%s\" failed: %s", pl->path, strerror(errno));
	fclose(f);

	log_printf("wrote \"%s\"", pl->path);
	print_place_info(pl);
	free(data);
}

int place_copy(struct Place **places, int *nplaces, int srcidx)
{
	log_printf("Copying place %d", srcidx);
	int n = (*nplaces)++;
	struct Place *arr = realloc(*places, sizeof(arr[0]) * (*nplaces));
	if (!arr)
		log_printf_abort("out of mem");
	*places = arr;

	int newnum = 0;
	for (int i = 0; i < n; i++) {
		if (arr[i].custom) {
			// Can have backslash on windows
			SDL_assert(strlen(arr[i].path) == strlen("custom_places/custom-12345.txt"));
			int oldnum = atoi(arr[i].path + strlen("custom_places/custom-"));
			newnum = max(newnum, oldnum+1);
		}
	}

	// Custom places are at end of places array and sorted by customnum
	memcpy(&arr[n], &arr[srcidx], sizeof arr[0]);
	sprintf(arr[n].path, "custom_places/custom-%05d.txt", newnum);
	arr[n].custom = true;
	place_save(&arr[n]);
	return n;
}

void place_delete(struct Place *places, int *nplaces, int delidx)
{
	log_printf("removing \"%s\"", places[delidx].path);
	if (remove(places[delidx].path) != 0)
		log_printf_abort("remove(\"%s\") failed: %s", places[delidx].path, strerror(errno));
	memmove(places + delidx, places + delidx + 1, (--*nplaces - delidx)*sizeof places[0]);
}
