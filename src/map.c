#include "map.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "glob.h"
#include "log.h"
#include "max.h"
#include "misc.h"

#define COMPILE_TIME_STRLEN(s) (sizeof(s)-1)

/*
Small language for specifying maps in files:
- 1x1 squares on xz plane with integer corner coordinates are built of parts like

	 --
	|  |
	 --

- content of square doesn't have to be spaces like above, can also be:
	- 'p': initial player place (need two of these in the map)
	- 'e': initial place for enemies (need one of these in the map)
	- 'j': jumper
- any of the '--' or '|' walls may be replaced with spaces, that means no wall
- each line is padded with spaces to have same length
- must have these walls:
	- wall at z=0 (first line)
	- wall at z=zsize (last line)
	- wall at x=0 (first character of lines 1,3,5,... in 0-based indexing)
	- wall at x=xsize (last character of those lines)
- NULL after last row
*/

#define MAX_LINE_LEN (COMPILE_TIME_STRLEN("|--")*MAX_MAPSIZE + COMPILE_TIME_STRLEN("|\n"))
#define MAX_LINE_COUNT (2*MAX_MAPSIZE + 1)

static bool read_line(char *line, size_t linesz, FILE *f)
{
	if (!fgets(line, linesz, f)) {
		if (ferror(f))
			log_printf_abort("can't read file: %s", strerror(errno));
		return false;  // EOF
	}

	int n = strlen(line);
	if (n > 0 && line[n-1] == '\n')
		line[n-1] = '\0';
	log_printf("%s", line);
	return true;
}

static int peek_one_char(FILE *f)
{
	int c = fgetc(f);
	if (c != EOF)
		ungetc(c, f);
	return c;
}

static void read_metadata(FILE *f, struct Map *map)
{
	strcpy(map->name, "(no name)");  // should never be actually used
	strcpy(map->origname, "");
	map->copycount = 0;
	map->sortkey = NAN;

	// Metadata section
	while (peek_one_char(f) != ' ') {
		char line[100 + sizeof map->name];
		if (!read_line(line, sizeof line, f))
			log_printf_abort("unexpected EOF while reading metadata");

		char *eq = strchr(line, '=');
		if (!eq)
			log_printf_abort("bad metadata line: %s", line);
		*eq = '\0';  // line is now the key of "key=value"
		const char *val = eq+1;

		if (strcmp(line, "Name") == 0)
			snprintf(map->name, sizeof map->name, "%s", val);
		else if (strcmp(line, "OriginalName") == 0)
			snprintf(map->origname, sizeof map->origname, "%s", val);
		else if (strcmp(line, "CopyCount") == 0)
			map->copycount = atoi(val);
		else if (strcmp(line, "SortKey") == 0)
			map->sortkey = atof(val);
		else
			log_printf_abort("unknown metadata key: %s", line);
	}

	if (!isfinite(map->sortkey))
		map->sortkey = map->name[0];  // a bit lol, but works for default maps
}

static const char *read_file_with_trailing_spaces_added(FILE *f, int *nlines)
{
	static char res[MAX_LINE_LEN*MAX_LINE_COUNT + 1];
	res[0] = '\0';

	char line[MAX_LINE_LEN + 1];
	int n = 0;
	while (read_line(line, sizeof line, f)) {
		while (strlen(line) < MAX_LINE_LEN-1)
			strcat(line, " ");
		strcat(line, "\n");
		strcat(res, line);

		SDL_assert(n < MAX_LINE_COUNT);
		n++;
	}

	*nlines = n;
	return res;
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
	struct Map *map;
	struct MapCoords loc;
	struct MapCoords *playerlocptr;   // pointer into map->playerlocs
};

static void parse_square_content(char c, struct SquareParsingState *st)
{
	switch(c) {
	case ' ':
		break;
	case 'e':
		SDL_assert(st->map->nenemylocs < MAX_ENEMIES);
		st->map->enemylocs[st->map->nenemylocs++] = st->loc;
		break;
	case 'j':
		SDL_assert(st->map->njumpers < MAX_JUMPERS);
		st->map->jumperlocs[st->map->njumpers++] = st->loc;
		break;
	case 'p':
		SDL_assert(st->map->playerlocs <= st->playerlocptr && st->playerlocptr < st->map->playerlocs + 2);
		*st->playerlocptr++ = st->loc;
		break;
	default:
		log_printf_abort("expected ' ', 'e', 'j' or 'p', got '%c'", c);
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

static const char *next_line(const char *s)
{
	const char *nl = strchr(s, '\n');
	SDL_assert(nl);
	return nl+1;
}

static void read_walls_and_players_and_enemies(FILE *f, struct Map *map)
{
	int nlines;
	const char *fdata = read_file_with_trailing_spaces_added(f, &nlines);

	/*
	 -----> x
	|
	|
	V
	z
	*/
	SDL_assert(nlines % 2 == 1 && nlines >= 3);   // e.g. { " -- ", "|  |", " -- " }, one more row means off by 2
	map->zsize = nlines/2;

	const char *secondline = next_line(fdata);
	int linelen = next_line(secondline) - secondline;
	while (secondline[linelen-1] != '|') {
		linelen--;
		SDL_assert(linelen > 0);
	}
	SDL_assert(linelen % 3 == 1 && linelen >= 4);  // e.g. "|  |", one more column means off by 3
	map->xsize = linelen / 3;

	struct SquareParsingState st = { .map = map, .playerlocptr = map->playerlocs };
	for (int z = 0; z < map->zsize; z++) {
		const char *line1 = fdata;
		const char *line2 = next_line(line1);
		const char *line3 = next_line(line2);
		fdata = line3;

		for (int x = 0; x < map->xsize; (x++, line1 += 3, line2 += 3, line3 += 3)) {
			st.loc = (struct MapCoords){x,z};

			bool top, bottom, left, right;
			top = parse_horizontal_wall_string(line1);
			parse_vertical_wall_string(line2, &left, &right, &st);
			bottom = parse_horizontal_wall_string(line3);

			// map must have surrounding left and top walls
			if (x == 0)
				SDL_assert(left);
			if (z == 0)
				SDL_assert(top);

			/*
			same for bottom and right, and when not last iteration of loop, we let
			another iteration of the loop to handle the wall
			*/
			if (x == map->xsize - 1)
				SDL_assert(right);
			else
				right = false;

			if (z == map->zsize - 1)
				SDL_assert(bottom);
			else
				bottom = false;

			if (top)    map_addwall(map, x,   z,   WALL_DIR_XY);
			if (bottom) map_addwall(map, x,   z+1, WALL_DIR_XY);
			if (left)   map_addwall(map, x,   z,   WALL_DIR_ZY);
			if (right)  map_addwall(map, x+1, z,   WALL_DIR_ZY);
		}
	}
}

static void read_map_from_file(struct Map *map, const char *path, bool custom)
{
	log_printf("Reading map from '%s'...", path);
	SDL_assert(strlen(path) < sizeof map->path);
	strcpy(map->path, path);

	if (custom) {
		// Find 12345 from custom_maps/12345-foo-bar.txt
		char digits[10];
		basename_without_extension(map->path, digits, sizeof digits);
		int n = atoi(digits);
		SDL_assert(n >= 0);
		map->num = n;
	} else
		map->num = -1;

	FILE *f = fopen(path, "r");
	if (!f)
		log_printf_abort("opening \"%s\" failed: %s", path, strerror(errno));

	read_metadata(f, map);
	read_walls_and_players_and_enemies(f, map);
	fclose(f);
}

static int compare_maps(const void *a, const void *b)
{
	const struct Map *ma = a, *mb = b;
	return (ma->sortkey > mb->sortkey) - (ma->sortkey < mb->sortkey);
}

struct Map *map_list(int *nmaps)
{
	glob_t gl;
	if (glob("assets/default_maps/*.txt", 0, NULL, &gl) != 0)
		log_printf_abort("default maps not found");

	int ndefault = gl.gl_pathc;
	int r = glob("custom_maps/*.txt", GLOB_APPEND, NULL, &gl);
	if (r != 0 && r != GLOB_NOMATCH)
		log_printf_abort("error while globbing custom maps");

	struct Map *maps = calloc(gl.gl_pathc, sizeof maps[0]);
	if (!maps)
		log_printf_abort("not enough memory for %d maps", (int)gl.gl_pathc);

	for (int i = 0; i < gl.gl_pathc; i++)
		read_map_from_file(&maps[i], gl.gl_pathv[i], i >= ndefault);
	globfree(&gl);

	*nmaps = gl.gl_pathc;
	qsort(maps, *nmaps, sizeof maps[0], compare_maps);
	return maps;
}

void map_addwall(struct Map *map, int x, int z, enum WallDirection dir)
{
	SDL_assert(map->nwalls < MAX_WALLS);
	struct Wall *w = &map->walls[map->nwalls++];
	w->startx = x;
	w->startz = z;
	w->dir = dir;
}

void map_movecontent(struct Map *map, int dx, int dz)
{
	for (int i = 0; i < map->nwalls; i++) {
		map->walls[i].startx += dx;
		map->walls[i].startz += dz;
	}
	for (int i = 0; i < 2; i++) {
		map->playerlocs[i].x += dx;
		map->playerlocs[i].z += dz;
	}
	for (int i = 0; i < map->nenemylocs; i++) {
		map->enemylocs[i].x += dx;
		map->enemylocs[i].z += dz;
	}
}

static void delete_walls_outside_the_map(struct Map *map)
{
	for (int i = map->nwalls - 1; i >= 0; i--) {
		if (
			map->walls[i].startx < 0 ||
			map->walls[i].startz < 0 ||
			map->walls[i].startx > map->xsize ||
			map->walls[i].startz > map->zsize ||
			(map->walls[i].dir == WALL_DIR_XY && map->walls[i].startx == map->xsize) ||
			(map->walls[i].dir == WALL_DIR_ZY && map->walls[i].startz == map->zsize)
		)
		{
			map->walls[i] = map->walls[--map->nwalls];
		}
	}
}

static void delete_duplicate_walls(struct Map *map)
{
	for (int i = 0; i < map->nwalls; i++) {
		for (int k = map->nwalls-1; k > i; k--) {
			if (wall_match(&map->walls[i], &map->walls[k]))
				map->walls[k] = map->walls[--map->nwalls];
		}
	}
}

static void add_missing_walls_around_edges(struct Map *map)
{
	bool foundx0[MAX_MAPSIZE] = {0};
	bool foundz0[MAX_MAPSIZE] = {0};
	bool foundxmax[MAX_MAPSIZE] = {0};
	bool foundzmax[MAX_MAPSIZE] = {0};
	for (const struct Wall *w = map->walls; w < &map->walls[map->nwalls]; w++) {
		switch(w->dir) {
			case WALL_DIR_XY:
				if (w->startz == 0)
					foundz0[w->startx] = true;
				if (w->startz == map->zsize)
					foundzmax[w->startx] = true;
				break;
			case WALL_DIR_ZY:
				if (w->startx == 0)
					foundx0[w->startz] = true;
				if (w->startx == map->xsize)
					foundxmax[w->startz] = true;
				break;
		}
	}
	for (int z = 0; z < map->zsize; z++) {
		if (!foundx0[z])
			map_addwall(map, 0, z, WALL_DIR_ZY);
		if (!foundxmax[z])
			map_addwall(map, map->xsize, z, WALL_DIR_ZY);
	}
	for (int x = 0; x < map->xsize; x++) {
		if (!foundz0[x])
			map_addwall(map, x, 0, WALL_DIR_XY);
		if (!foundzmax[x])
			map_addwall(map, x, map->zsize, WALL_DIR_XY);
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
static void manhattan_spiral(struct MapCoords *p, struct MapCoords center)
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

static bool point_is_available(const struct Map *map, const struct MapCoords p)
{
	if (p.x < 0 || p.x >= map->xsize || p.z < 0 || p.z >= map->zsize)
		return false;

	for (int i=0; i<2; i++) {
		if (map->playerlocs[i].x == p.x && map->playerlocs[i].z == p.z)
			return false;
	}
	for (int i = 0; i < map->nenemylocs; i++) {
		if (map->enemylocs[i].x == p.x && map->enemylocs[i].z == p.z)
			return false;
	}
	return true;
}

static void fix_location(const struct Map *map, struct MapCoords *ptr)
{
	SDL_assert(2 + map->nenemylocs <= map->xsize*map->zsize);

	// Make it temporary disappear from the map, so we won't see it when looking for free place
	// Prevents it from always moving, but still moves in case of overlaps
	// Also ensures there's enough room for it
	struct MapCoords hint = *ptr;
	*ptr = (struct MapCoords){ -1, -1 };

	struct MapCoords p = hint;
	while (!point_is_available(map, p))
		manhattan_spiral(&p, hint);
	*ptr = p;
}

static void ensure_players_and_enemies_are_inside_the_map_and_dont_overlap(struct Map *map)
{
	clamp(&map->nenemylocs, 0, map->xsize*map->zsize - 2);  // leave room for 2 players

	for (int i=0; i<2; i++)
		fix_location(map, &map->playerlocs[i]);
	for (int i = 0; i < map->nenemylocs; i++)
		fix_location(map, &map->enemylocs[i]);
}

void map_fix(struct Map *map)
{
	SDL_assert(2 <= map->xsize && map->xsize <= MAX_MAPSIZE);
	SDL_assert(2 <= map->zsize && map->zsize <= MAX_MAPSIZE);

	delete_walls_outside_the_map(map);
	delete_duplicate_walls(map);
	add_missing_walls_around_edges(map);
	ensure_players_and_enemies_are_inside_the_map_and_dont_overlap(map);
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

static void get_map_path(const struct Map *map, char *res)
{
	SDL_assert(map->num != -1);
	sprintf(res, "custom_maps/%05d-", map->num);
	while(*res) res++;

	// res won't be overflown, map names are short
	static_assert(sizeof map->name < 50, "time to check for buffer overruns! yay...");
	for (const char *src = map->name; *src; utf8_next_const(&src))
	{
		if (('a' <= *src && *src <= 'z')
				|| ('A' <= *src && *src <= 'Z')
				|| ('0' <= *src && *src <= '9'))
			*res++ = tolower(*src);
		else
			*res++ = '-';
	}
	strcpy(res, ".txt");
}

static void rename_file_if_needed(struct Map *map)
{
	char newpath[sizeof map->path];
	get_map_path(map, newpath);
	if (!strcmp(map->path, newpath))
		return;

	if (rename(map->path, newpath) == 0) {
		log_printf("Renamed: \"%s\" --> \"%s\"", map->path, newpath);
		strcpy(map->path, newpath);
	} else
		log_printf("Rename failed: \"%s\" --> \"%s\" (%s)", map->path, newpath, strerror(errno));
}

void map_save(struct Map *map)
{
	int linesz = strlen("|--")*map->xsize + strlen("|") + 1;
	int nlines = 2*map->zsize + 1;

	char *data = malloc(linesz*nlines);
	if (!data)
		log_printf_abort("not enough memory");

	memset(data, ' ', linesz*nlines);
	for (int lineno = 0; lineno < nlines; lineno++)
		data[lineno*linesz + (linesz-1)] = '\0';

	for (const struct Wall *w = map->walls; w < &map->walls[map->nwalls]; w++) {
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
		set_char(data, linesz, nlines, map->playerlocs[i].x, map->playerlocs[i].z, 'p', 1);
	for (int i = 0; i < map->nenemylocs; i++)
		set_char(data, linesz, nlines, map->enemylocs[i].x, map->enemylocs[i].z, 'e', 1);
	for (int i = 0; i < map->njumpers; i++)
		set_char(data, linesz, nlines, map->jumperlocs[i].x, map->jumperlocs[i].z, 'j', 1);

	log_printf("Writing to \"%s\"", map->path);
	SDL_assert(strstr(map->path, "custom_maps") == map->path);  // map->path is like "custom_maps/00006-foo-bar.txt"
	my_mkdir("custom_maps");
	FILE *f = fopen(map->path, "w");
	if (!f)
		log_printf_abort("opening \"%s\" failed: %s", map->path, strerror(errno));

	#define write_and_log_line(...) do{ \
		log_printf(__VA_ARGS__); \
		if (fprintf(f, __VA_ARGS__) < 0 || fprintf(f, "\n") < 0) \
			log_printf_abort("writing to \"%s\" failed: %s", map->path, strerror(errno)); \
	} while(0)

	write_and_log_line("Name=%s", map->name);
	write_and_log_line("OriginalName=%s", map->origname);
	write_and_log_line("CopyCount=%d", map->copycount);
	write_and_log_line("SortKey=%.10f", map->sortkey);

	// log get truncated if all data in one printf, maybe limitation in sdl2 logging
	for (int i = 0; i < nlines; i++)
		write_and_log_line("%s", data+(i*linesz));

	#undef write_and_log_line
	fclose(f);
	free(data);
	rename_file_if_needed(map);
}

void map_update_sortkey(struct Map *maps, int nmaps, int idx)
{
	SDL_assert(maps[idx].num != -1);  // must be custom map
	SDL_assert(0 <= idx && idx < nmaps);
	if (nmaps < 2)
		return;

	if (idx == 0)
		maps[idx].sortkey = maps[1].sortkey - 1;
	else if (idx == nmaps-1)
		maps[idx].sortkey = maps[nmaps-2].sortkey + 1;
	else
		maps[idx].sortkey = (maps[idx-1].sortkey + maps[idx+1].sortkey)/2;

	for (int i = 0; i+1 < nmaps; i++){
		// could reach equality if run out of precision
		SDL_assert(maps[i].sortkey <= maps[i+1].sortkey);
	}

	map_save(&maps[idx]);
}

// returns false for non-custom places
static bool has_default_copy_name(const struct Map *m)
{
	char defaultname[sizeof m->name];
	snprintf(defaultname, sizeof defaultname, "Copy %d: %s", m->copycount, m->origname);
	return (strcmp(m->name, defaultname) == 0);
}

int map_copy(struct Map **maps, int *nmaps, int srcidx)
{
	log_printf("Copying map \"%s\" at index %d", (*maps)[srcidx].name, srcidx);
	*maps = realloc(*maps, sizeof((*maps)[0]) * (*nmaps + 1));
	if (!*maps)
		log_printf_abort("out of mem");
	struct Map *ptr = &(*maps)[srcidx+1];  // ptr[-1] is source map

	const char *origname;
	if (has_default_copy_name(&ptr[-1]))
		origname = ptr[-1].origname;
	else
		origname = ptr[-1].name;

	int maxnum = 0;
	int maxcopycount = 0;
	for (struct Map *m = *maps; m < *maps + *nmaps; m++) {
		if (m->num != -1) {  // custom map
			maxnum = max(maxnum, m->num);
			if (strcmp(m->origname, origname) == 0)
				maxcopycount = max(maxcopycount, m->copycount);
		}
	}

	memmove(ptr, ptr-1, ((*nmaps)++ - srcidx)*sizeof(*ptr));

	ptr->num = maxnum+1;
	get_map_path(ptr, ptr->path);

	strcpy(ptr->origname, origname);
	ptr->copycount = maxcopycount+1;
	snprintf(ptr->name, sizeof ptr->name, "Copy %d: %s", ptr->copycount, ptr->origname);

	map_update_sortkey(*maps, *nmaps, ptr - *maps);
	map_save(ptr);
	return ptr - *maps;
}

void map_delete(struct Map *maps, int *nmaps, int delidx)
{
	log_printf("removing \"%s\"", maps[delidx].path);
	if (remove(maps[delidx].path) != 0)
		log_printf_abort("remove(\"%s\") failed: %s", maps[delidx].path, strerror(errno));
	memmove(maps + delidx, maps + delidx + 1, (--*nmaps - delidx)*sizeof maps[0]);
}
