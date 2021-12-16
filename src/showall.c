#include "showall.h"
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "ellipsoid.h"
#include "interval.h"
#include "mathstuff.h"
#include "max.h"
#include "wall.h"
#include "log.h"

// fitting too much stuff into an int
typedef int ID;
#define ID_TYPE_ELLIPSOID 0
#define ID_TYPE_WALL 1
#define ID_TYPE(id) ((id) & 1)
#define ID_INDEX(id) ((id) >> 1)
#define ID_NEW(type, idx) ((type) | ((idx) << 1))

// length of array indexed by id
#define ID_ARRAYLEN max( \
	ID_NEW(ID_TYPE_ELLIPSOID, MAX_ELLIPSOIDS-1) + 1, \
	ID_NEW(ID_TYPE_WALL, MAX_WALLS-1) + 1 \
)

struct Info {
	// dependencies must be displayed first, they go to behind the ellipsoid or wall
	ID deps[MAX_WALLS + MAX_ELLIPSOIDS];
	int ndeps;

	SDL_Rect bbox;       // bounding box
	bool insortedarray;  // for sorting infos to display them in correct order

	// only for walls
	struct Rect rect;
	struct RectCache rcache;
	bool highlight;
};

struct ShowingState {
	const struct Camera *cam;
	const struct Wall *walls;           // indexed by ID_INDEX(wall id)
	const struct Ellipsoid *els;        // indexed by ID_INDEX(ellipsoid id)
	struct Info infos[ID_ARRAYLEN];     // indexed by id

	ID visible[MAX_WALLS + MAX_ELLIPSOIDS];
	int nvisible;
};

static void add_ellipsoid_if_visible(struct ShowingState *st, int idx)
{
	if (ellipsoid_is_visible(&st->els[idx], st->cam)) {
		ID id = ID_NEW(ID_TYPE_ELLIPSOID, idx);
		st->visible[st->nvisible++] = id;
		st->infos[id].ndeps = 0;
		st->infos[id].bbox = ellipsoid_bounding_box(&st->els[idx], st->cam);
		st->infos[id].insortedarray = false;
	}
}

static void add_wall_if_visible(struct ShowingState *st, int idx)
{
	struct Rect r = wall_to_rect(&st->walls[idx]);
	struct RectCache rcache;
	if (rect_visible_fillcache(&r, st->cam, &rcache)) {
		ID id = ID_NEW(ID_TYPE_WALL, idx);
		st->visible[st->nvisible++] = id;
		st->infos[id].ndeps = 0;
		st->infos[id].bbox = rcache.bbox;
		st->infos[id].insortedarray = false;
		st->infos[id].rect = r;
		st->infos[id].rcache = rcache;
		st->infos[id].highlight = false;
	}
}

static void add_dependency(struct ShowingState *st, ID before, ID after)
{
	for (int i = 0; i < st->infos[after].ndeps; i++) {
		if (st->infos[after].deps[i] == before)
			return;
	}

	st->infos[after].deps[st->infos[after].ndeps++] = before;
}

float get_z_in_camera_coordinates(const struct ShowingState *st, ID id, float xzr, float yzr)
{
	struct Rect r;
	switch(ID_TYPE(id)) {
	case ID_TYPE_WALL:
		r = wall_to_rect(&st->walls[ID_INDEX(id)]);
		break;
	case ID_TYPE_ELLIPSOID:
		r = ellipsoid_get_sort_rect(&st->els[ID_INDEX(id)], st->cam);
		break;
	}
	return rect_get_camcoords_z(&r, st->cam, xzr, yzr);
}

static void setup_dependencies(struct ShowingState *st)
{
	for (int x = 0; x < st->cam->surface->w; x++) {
		float xzr = camera_screenx_to_xzr(st->cam, x);

		static struct Match {
			ID id;
			SDL_Rect bbox;
		} matches[MAX_WALLS + MAX_ELLIPSOIDS];
		int nmatches = 0;

		for (int i = 0; i < st->nvisible; i++) {
			ID id = st->visible[i];
			SDL_Rect bbox = st->infos[id].bbox;
			if (bbox.x < x && x < bbox.x+bbox.w)
				matches[nmatches++] = (struct Match){ id, bbox };
		}

		for (int i = 0; i < nmatches; i++) {
			for (int k = 0; k < i; k++) {
				struct Match m1 = matches[i];
				struct Match m2 = matches[k];

				int ystart = max(m1.bbox.y, m2.bbox.y);
				int yend = min(m1.bbox.y + m1.bbox.h, m2.bbox.y + m2.bbox.h);
				if (ystart > yend)
					continue;
				float yzr = camera_screeny_to_yzr(st->cam, (ystart + yend)*0.5f);
				float z1 = get_z_in_camera_coordinates(st, m1.id, xzr, yzr);
				float z2 = get_z_in_camera_coordinates(st, m2.id, xzr, yzr);
				if (z1 < z2)
					add_dependency(st, m1.id, m2.id);
				else
					add_dependency(st, m2.id, m1.id);
			}
		}
	}

}

#if 0
static void debug_print_dependencies(const struct ShowingState *st)
{
	printf("\ndependency dump:\n");
	for (int i = 0; i < st->nvisible; i++) {
		ID id = st->visible[i];
		if (st->infos[id].ndeps == 0)
			continue;

		printf("   %-3d --> ", id);
		for (int d = 0; d < st->infos[id].ndeps; d++)
			printf("%s%d", (d?",":""), st->infos[id].deps[d]);
		printf("\n");
	}
	printf("\n");
}
#endif

static void add_dependencies_and_id_to_sorted_array(struct ShowingState *st, ID **ptr, ID id, int depth)
{
	if (depth > MAX_WALLS + MAX_ELLIPSOIDS) {
		// don't know when this could happen, but prevent disasters
		log_printf("hitting recursion depth limit, something weird is happening");
		return;
	}
	if (st->infos[id].insortedarray)
		return;

	// setting this early avoids infinite recursion in case dependency graph has a cycle
	st->infos[id].insortedarray = true;   

	for (int i = 0; i < st->infos[id].ndeps; i++)
		add_dependencies_and_id_to_sorted_array(st, ptr, st->infos[id].deps[i], depth+1);

	*(*ptr)++ = id;
}

// In which order should walls and ellipsoids be shown?
static void create_sorted_array(struct ShowingState *st, ID *sorted)
{
	ID *ptr = sorted;
	for (int i = 0; i < st->nvisible; i++)
		add_dependencies_and_id_to_sorted_array(st, &ptr, st->visible[i], 0);
	SDL_assert(sorted + st->nvisible == ptr);
}

static bool get_xminmax(struct ShowingState *st, ID id, int y, int *xmin, int *xmax)
{
	switch(ID_TYPE(id)) {
		case ID_TYPE_ELLIPSOID: return ellipsoid_xminmax(&st->els[ID_INDEX(id)], st->cam, y, xmin, xmax);
		case ID_TYPE_WALL: return rect_xminmax(&st->infos[id].rcache, y, xmin, xmax);
	}
	return false;  // compiler = happy
}

static void draw_row(const struct ShowingState *st, int y, ID id, int xmin, int xmax)
{
	switch(ID_TYPE(id)) {
	case ID_TYPE_ELLIPSOID:
		ellipsoid_drawrow(&st->els[ID_INDEX(id)], st->cam, y, xmin, xmax);
		break;
	case ID_TYPE_WALL:
		rect_drawrow(&st->infos[id].rcache, y, xmin, xmax, st->infos[id].highlight);
		break;
	}
}

#define ArrayLen(arr) ( sizeof(arr) / sizeof((arr)[0]) )

void show_all(
	const struct Wall *walls, int nwalls,
	const int *highlightwalls,
	const struct Ellipsoid *els, int nels,
	const struct Camera *cam)
{
	SDL_assert(nwalls <= MAX_WALLS);
	SDL_assert(nels <= MAX_ELLIPSOIDS);

	// static to keep stack usage down
	static struct ShowingState st;
	st.cam = cam;
	st.walls = walls;
	st.els = els;
	st.nvisible = 0;

	for (int i = 0; i < nels; i++)
		add_ellipsoid_if_visible(&st, i);
	for (int i = 0; i < nwalls; i++)
		add_wall_if_visible(&st, i);
	for (const int *i = highlightwalls; *i != -1; i++)
		st.infos[ID_NEW(ID_TYPE_WALL, *i)].highlight = true;

	setup_dependencies(&st);

	static ID sorted[ArrayLen(st.visible)];
	create_sorted_array(&st, sorted);

	for (int y = 0; y < cam->surface->h; y++) {
		static struct Interval intervals[ArrayLen(sorted)];
		int nintervals = 0;

		for (int i = 0; i < st.nvisible; i++) {
			ID id = sorted[i];
			if (!( st.infos[id].bbox.y <= y && y <= st.infos[id].bbox.y + st.infos[id].bbox.h ))
				continue;

			int xmin, xmax;
			if (get_xminmax(&st, id, y, &xmin, &xmax)) {
				SDL_assert(xmin <= xmax);
				intervals[nintervals++] = (struct Interval){
					.start = xmin,
					.end = xmax,
					.id = id,
					.allowoverlap = (ID_TYPE(id) == ID_TYPE_WALL),
				};
			}
		}

		static struct Interval nonoverlap[INTERVAL_NON_OVERLAPPING_MAX(ArrayLen(intervals))];
		int nnonoverlap = interval_non_overlapping(intervals, nintervals, nonoverlap);

		for (int i = 0; i < nnonoverlap; i++)
			draw_row(&st, y, nonoverlap[i].id, nonoverlap[i].start, nonoverlap[i].end);
	}
}
