#include "place.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "glob.h"
#include "log.h"
#include "max.h"
#include "mathstuff.h"
#include "misc.h"

#define COMPILE_TIME_STRLEN(s) (sizeof(s)-1)

/*
Small language for specifying places in files:
- 1x1 squares on xz plane with integer corner coordinates are built of parts like

	 --
	|  |
	 --

- content of square doesn't have to be spaces like above, can also be:
	- 'p': initial player place (need two of these in the place)
	- 'e': initial place for enemies (need one of these in the place)
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

void place_addwall(struct Place *pl, int x, int z, enum WallDirection dir)
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
		SDL_assert(st->place->nenemylocs < MAX_ENEMIES);
		st->place->enemylocs[st->place->nenemylocs++] = st->loc;
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
	log_printf("    %d enemy locations", pl->nenemylocs);
	for (int i = 0; i < 2; i++)
		log_printf("    player %d goes to x=%d z=%d", i, pl->playerlocs[i].x, pl->playerlocs[i].z);
}

static const char *next_line(const char *s)
{
	char *nl = strchr(s, '\n');
	SDL_assert(nl);
	return nl+1;
}

static void read_place_from_file(struct Place *pl, const char *path, bool custom)
{
	log_printf("Reading place from '%s'...", path);
	SDL_assert(strlen(path) < sizeof pl->path);
	strcpy(pl->path, path);
	pl->custom = custom;

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

			if (top)    place_addwall(pl, x,   z,   WALL_DIR_XY);
			if (bottom) place_addwall(pl, x,   z+1, WALL_DIR_XY);
			if (left)   place_addwall(pl, x,   z,   WALL_DIR_ZY);
			if (right)  place_addwall(pl, x+1, z,   WALL_DIR_ZY);
		}
	}

	print_place_info(pl);
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

void place_movecontent(struct Place *pl, int dx, int dz)
{
	for (int i = 0; i < pl->nwalls; i++) {
		pl->walls[i].startx += dx;
		pl->walls[i].startz += dz;
		wall_init(&pl->walls[i]);
	}
	for (int i = 0; i < 2; i++) {
		pl->playerlocs[i].x += dx;
		pl->playerlocs[i].z += dz;
	}
	for (int i = 0; i < pl->nenemylocs; i++) {
		pl->enemylocs[i].x += dx;
		pl->enemylocs[i].z += dz;
	}
}

static void delete_walls_outside_the_place(struct Place *pl)
{
	for (int i = pl->nwalls - 1; i >= 0; i--) {
		if (
			pl->walls[i].startx < 0 ||
			pl->walls[i].startz < 0 ||
			pl->walls[i].startx > pl->xsize ||
			pl->walls[i].startz > pl->zsize ||
			(pl->walls[i].dir == WALL_DIR_XY && pl->walls[i].startx == pl->xsize) ||
			(pl->walls[i].dir == WALL_DIR_ZY && pl->walls[i].startz == pl->zsize)
		)
		{
			pl->walls[i] = pl->walls[--pl->nwalls];
		}
	}
}

static void delete_duplicate_walls(struct Place *pl)
{
	for (int i = 0; i < pl->nwalls; i++) {
		for (int k = pl->nwalls-1; k > i; k--) {
			if (wall_match(&pl->walls[i], &pl->walls[k]))
				pl->walls[k] = pl->walls[--pl->nwalls];
		}
	}
}

static void add_missing_walls_around_edges(struct Place *pl)
{
	bool foundx0[MAX_PLACE_SIZE] = {0};
	bool foundz0[MAX_PLACE_SIZE] = {0};
	bool foundxmax[MAX_PLACE_SIZE] = {0};
	bool foundzmax[MAX_PLACE_SIZE] = {0};
	for (const struct Wall *w = pl->walls; w < &pl->walls[pl->nwalls]; w++) {
		switch(w->dir) {
			case WALL_DIR_XY:
				if (w->startz == 0)
					foundz0[w->startx] = true;
				if (w->startz == pl->zsize)
					foundzmax[w->startx] = true;
				break;
			case WALL_DIR_ZY:
				if (w->startx == 0)
					foundx0[w->startz] = true;
				if (w->startx == pl->xsize)
					foundxmax[w->startz] = true;
				break;
		}
	}
	for (int z = 0; z < pl->zsize; z++) {
		if (!foundx0[z])
			place_addwall(pl, 0, z, WALL_DIR_ZY);
		if (!foundxmax[z])
			place_addwall(pl, pl->xsize, z, WALL_DIR_ZY);
	}
	for (int x = 0; x < pl->xsize; x++) {
		if (!foundz0[x])
			place_addwall(pl, x, 0, WALL_DIR_XY);
		if (!foundzmax[x])
			place_addwall(pl, x, pl->zsize, WALL_DIR_XY);
	}
}

/*
When called repeatedly, spirals around center like this (0 = center = called 0 times):

	   z
	/|\        .
	 |      7   '.
	 |   8  2  6  14
	 |9  3  0  1  5  13
	 |   10 4  12
	 |      11
	 |
	  ------------->  x

Note that manhattan distance (see wikipedia) between center and the spiral points
never decreases, hence the name.
*/
static void manhattan_spiral(struct PlaceCoords *p, struct PlaceCoords center)
{
	if (p->x > center.x && p->z >= center.z) {
		p->x--;
		p->z++;
	} else if (p->x <= center.x && p->z > center.z) {
		p->x--;
		p->z--;
	} else if (p->x < center.x && p->z <= center.z) {
		p->x++;
		p->z--;
	} else if (p->x >= center.x && p->z < center.z) {
		p->x++;
		p->z++;
	}

	if (p->x >= center.x && p->z == center.z) {
		// Move further away from center
		p->x++;
	}
}

static bool point_is_available(const struct Place *pl, const struct PlaceCoords p)
{
	if (p.x < 0 || p.x >= pl->xsize || p.z < 0 || p.z >= pl->zsize)
		return false;

	for (int i=0; i<2; i++) {
		if (pl->playerlocs[i].x == p.x && pl->playerlocs[i].z == p.z)
			return false;
	}
	for (int i = 0; i < pl->nenemylocs; i++) {
		if (pl->enemylocs[i].x == p.x && pl->enemylocs[i].z == p.z)
			return false;
	}
	return true;
}

static struct PlaceCoords findempty_without_the_check(const struct Place *pl, struct PlaceCoords hint)
{
	struct PlaceCoords p = hint;
	while (!point_is_available(pl, p))
		manhattan_spiral(&p, hint);
	return p;
}

struct PlaceCoords place_findempty(const struct Place *pl, struct PlaceCoords hint)
{
	SDL_assert(2 + pl->nenemylocs < pl->xsize*pl->zsize);   // must not be full
	return findempty_without_the_check(pl, hint);
}

static void fix_location(const struct Place *pl, struct PlaceCoords *ptr)
{
	SDL_assert(2 + pl->nenemylocs <= pl->xsize*pl->zsize);
	struct PlaceCoords hint = *ptr;

	// Make it temporary disappear from the world, so we won't see it when looking for free place
	// Prevents it from always moving, but still moves in case of overlaps
	// Also ensures there's enough room for it
	*ptr = (struct PlaceCoords){ -1, -1 };
	*ptr = findempty_without_the_check(pl, hint);
}

static void ensure_players_and_enemies_are_inside_the_place_and_dont_overlap(struct Place *pl)
{
	clamp(&pl->nenemylocs, 0, pl->xsize*pl->zsize - 2);  // leave room for 2 players

	for (int i=0; i<2; i++)
		fix_location(pl, &pl->playerlocs[i]);
	for (int i = 0; i < pl->nenemylocs; i++)
		fix_location(pl, &pl->enemylocs[i]);
}

void place_fix(struct Place *pl)
{
	SDL_assert(2 <= pl->xsize && pl->xsize <= MAX_PLACE_SIZE);
	SDL_assert(2 <= pl->zsize && pl->zsize <= MAX_PLACE_SIZE);

	delete_walls_outside_the_place(pl);
	delete_duplicate_walls(pl);
	add_missing_walls_around_edges(pl);
	ensure_players_and_enemies_are_inside_the_place_and_dont_overlap(pl);
}

static void set_char(char *data, int linesz, int nlines, int x, int z, char c, int offset)
{
	// The asserts in this function help with finding weird bugs, don't delete
	int lineno = 2*z + (c != '-');
	SDL_assert(0 <= lineno && lineno < nlines);
	int idx = lineno*linesz + strlen("|--")*x + offset;
	SDL_assert(idx < linesz*nlines);
	data[idx] = c;
}

void place_save(const struct Place *pl)
{
	SDL_assert(pl->custom);
	int linesz = strlen("|--")*pl->xsize + strlen("|\n");
	int nlines = 2*pl->zsize + 1;

	char *data = malloc(linesz*nlines + 1);
	if (!data)
		log_printf_abort("not enough memory");

	data[linesz*nlines] = '\0';
	memset(data, ' ', linesz*nlines);
	for (int lineno = 0; lineno < nlines; lineno++)
		data[lineno*linesz + (linesz-1)] = '\n';

	for (const struct Wall *w = pl->walls; w < &pl->walls[pl->nwalls]; w++) {
		switch(w->dir) {
		case WALL_DIR_XY:
			set_char(data, linesz, nlines, w->startx, w->startz, '-', 1);
			set_char(data, linesz, nlines, w->startx, w->startz, '-', 2);
			break;
		case WALL_DIR_ZY:
			set_char(data, linesz, nlines, w->startx, w->startz, '|', 0);
			break;
		}
	}

	for (int i = 0; i < 2; i++)
		set_char(data, linesz, nlines, pl->playerlocs[i].x, pl->playerlocs[i].z, 'p', 1);
	for (int i = 0; i < pl->nenemylocs; i++)
		set_char(data, linesz, nlines, pl->enemylocs[i].x, pl->enemylocs[i].z, 'e', 1);

	// Can get truncated if all data in one log message, maybe limitation in sdl2 logging
	log_printf("Writing to \"%s\"", pl->path);
	for (int i = 0; i < nlines; i++)
		log_printf("%.*s", linesz, data+(i*linesz));

	misc_mkdir("custom_places");  // pl->path is like "custom_places/custom-00006.txt"
	FILE *f = fopen(pl->path, "w");
	if (!f)
		log_printf_abort("opening \"%s\" failed: %s", pl->path, strerror(errno));
	if (fwrite(data, 1, linesz*nlines, f) != linesz*nlines)
		log_printf_abort("writing to \"%s\" failed: %s", pl->path, strerror(errno));
	fclose(f);

	print_place_info(pl);
	free(data);
}

static void remove_prefix(const char **s, const char *pre)
{
	SDL_assert(strstr(*s, pre) == *s);
	*s += strlen(pre);
}

int place_copy(struct Place **places, int *nplaces, int srcidx)
{
	log_printf("Copying place %d", srcidx);
	int n = (*nplaces)++;
	struct Place *arr = realloc(*places, sizeof(arr[0]) * (*nplaces));
	if (!arr)
		log_printf_abort("out of mem");

	int newnum = 0;
	for (int i = 0; i < n; i++) {
		if (arr[i].custom) {
			// Parse custom_places/custom-12345.txt
			const char *ptr = arr[i].path;
			remove_prefix(&ptr, "custom_places");
			SDL_assert(*ptr == '/' || *ptr == '\\');
			ptr++;
			remove_prefix(&ptr, "custom-");
			newnum = max(newnum, atoi(ptr)+1);
		}
	}

	arr[n] = arr[srcidx];
	sprintf(arr[n].path, "custom_places/custom-%05d.txt", newnum);
	arr[n].custom = true;
	place_save(&arr[n]);

	*places = arr;
	return n;
}

void place_delete(struct Place *places, int *nplaces, int delidx)
{
	log_printf("removing \"%s\"", places[delidx].path);
	if (remove(places[delidx].path) != 0)
		log_printf_abort("remove(\"%s\") failed: %s", places[delidx].path, strerror(errno));
	memmove(places + delidx, places + delidx + 1, (--*nplaces - delidx)*sizeof places[0]);
}
