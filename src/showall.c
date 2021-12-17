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

// fitting too much stuff into an integer
typedef short ID;
#define ID_TYPE_ELLIPSOID 0
#define ID_TYPE_WALL 1
#define ID_TYPE(id) ((id) & 1)
#define ID_INDEX(id) ((id) >> 1)
#define ID_NEW(type, idx) ((type) | ((idx) << 1))

// length of array indexed by id
#define ARRAYLEN_INDEXED_BY_ID max( \
	ID_NEW(ID_TYPE_ELLIPSOID, MAX_ELLIPSOIDS-1) + 1, \
	ID_NEW(ID_TYPE_WALL, MAX_WALLS-1) + 1 \
)

// length of array containing ids
#define ARRAYLEN_CONTAINING_ID (MAX_WALLS + MAX_ELLIPSOIDS)

struct Info {
	// dependencies must be displayed first, they go to behind the ellipsoid or wall
	ID deps[MAX_WALLS + MAX_ELLIPSOIDS];
	int ndeps;

	SDL_Rect bbox;       // bounding box
	struct Rect sortrect;
	bool insortedarray;  // for sorting infos to display them in correct order

	struct RectCache rcache;
	bool highlight;
};

struct ShowingState {
	const struct Camera *cam;
	const struct Wall *walls;           // indexed by ID_INDEX(wall id)
	const struct Ellipsoid *els;        // indexed by ID_INDEX(ellipsoid id)
	struct Info infos[ARRAYLEN_INDEXED_BY_ID];

	ID visible[ARRAYLEN_CONTAINING_ID];
	int nvisible;
};

static void add_ellipsoid_if_visible(struct ShowingState *st, int idx)
{
	if (ellipsoid_is_visible(&st->els[idx], st->cam)) {
		ID id = ID_NEW(ID_TYPE_ELLIPSOID, idx);
		st->visible[st->nvisible++] = id;
		st->infos[id].ndeps = 0;
		st->infos[id].bbox = ellipsoid_bounding_box(&st->els[idx], st->cam);
		st->infos[id].sortrect = ellipsoid_get_sort_rect(&st->els[idx], st->cam);
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
		st->infos[id].sortrect = r;
		st->infos[id].insortedarray = false;
		st->infos[id].rcache = rcache;
		st->infos[id].highlight = false;
	}
}

static void __attribute__((noinline)) add_dependency(struct ShowingState *st, ID before, ID after)
{
	for (int i = 0; i < st->infos[after].ndeps; i++) {
		if (st->infos[after].deps[i] == before)
			return;
	}
	st->infos[after].deps[st->infos[after].ndeps++] = before;
}

static int __attribute__((noinline)) create_xlist(struct ShowingState *st, int *xlist)
{
	bool xcoords[CAMERA_SCREEN_WIDTH] = {0};
	for (int i = 0; i < st->nvisible; i++) {
		SDL_Rect bbox = st->infos[st->visible[i]].bbox;
		int lo = bbox.x, hi = bbox.x+bbox.w;
		SDL_assert(0 <= lo && lo < CAMERA_SCREEN_WIDTH);
		SDL_assert(0 <= hi && hi < CAMERA_SCREEN_WIDTH);
		xcoords[lo] = true;
		xcoords[hi] = true;
	}

	int xlistlen = 0;
	for (int x = 0; x < sizeof(xcoords)/sizeof(xcoords[0]); x++)
		if (xcoords[x])
			xlist[xlistlen++] = x;
	return xlistlen;
}

static void debug_print_dependencies(const struct ShowingState *st)
{
	log_printf("dependency dump:");
	for (int i = 0; i < st->nvisible; i++) {
		ID id = st->visible[i];
		if (st->infos[id].ndeps == 0)
			continue;

		log_printf("  %d", id);
		for (int d = 0; d < st->infos[id].ndeps; d++)
			log_printf("  `--> %d", st->infos[id].deps[d]);
	}
}

static void __attribute__((noinline)) setup_dependencies(
	struct ShowingState *st,
	const ID (*const objects_by_x)[CAMERA_SCREEN_WIDTH][ARRAYLEN_CONTAINING_ID],
	const int *nobjects_by_x)
{
	int xlist[CAMERA_SCREEN_WIDTH];
	int xlistlen = create_xlist(st, xlist);

	// It should be enough to check in the middles of intervals between bboxes
	for (int xidx = 1; xidx < xlistlen; xidx++) {
		int x = (xlist[xidx-1] + xlist[xidx])/2;
		float xzr = camera_screenx_to_xzr(st->cam, x);

		for (int i = 0; i < nobjects_by_x[x]; i++)
			for (int k = 0; k < i; k++)
			{
				ID id1 = (*objects_by_x)[x][i];
				ID id2 = (*objects_by_x)[x][k];
				if (ID_TYPE(id1) == ID_TYPE_WALL && ID_TYPE(id2) == ID_TYPE_WALL)
					continue;

				SDL_Rect bbox1 = st->infos[id1].bbox;

				int ystart = max(
					st->infos[id1].bbox.y,
					st->infos[id2].bbox.y);
				int yend = min(
					st->infos[id1].bbox.y + st->infos[id1].bbox.h,
					st->infos[id2].bbox.y + st->infos[id2].bbox.h);
				if (ystart > yend)
					continue;

				float yzr = camera_screeny_to_yzr(st->cam, (ystart + yend)/2);
				float z1 = rect_get_camcoords_z(&st->infos[id1].sortrect, st->cam, xzr, yzr);
				float z2 = rect_get_camcoords_z(&st->infos[id2].sortrect, st->cam, xzr, yzr);
				if (z1 < z2)
					add_dependency(st, id1, id2);
				else
					add_dependency(st, id2, id1);
			}
	}

	//debug_print_dependencies(st);
}

// In which order should walls and ellipsoids be shown?
static void __attribute__((noinline)) create_sorted_array(struct ShowingState *st, ID *sorted)
{
	static ID todo[MAX_WALLS + MAX_ELLIPSOIDS];
	int ntodo = st->nvisible;
	memcpy(todo, st->visible, ntodo*sizeof(todo[0]));

	// Standard topological sort algorithm. I learned it from Python's toposort module
	while (ntodo > 0) {
		bool stuck = true;
		for (int i = ntodo-1; i >= 0; i--) {
			if (st->infos[todo[i]].ndeps == 0) {
				*sorted++ = todo[i];
				st->infos[todo[i]].insortedarray = true;
				todo[i] = todo[--ntodo];
				stuck = false;
			}
		}
		if (stuck) {
			log_printf("dependency cycle detected");
			debug_print_dependencies(st);
			st->infos[todo[0]].ndeps--;
			continue;
		}
		for (int i = 0; i < ntodo; i++) {
			struct Info *info = &st->infos[todo[i]];
			for (int k = info->ndeps-1; k >= 0; k--) {
				if (st->infos[info->deps[k]].insortedarray)
					info->deps[k] = info->deps[--info->ndeps];
			}
		}
	}
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

static void __attribute__((noinline)) add_visible_objects(
	struct ShowingState *st,
	const struct Wall *walls, int nwalls,
	const int *highlightwalls,
	const struct Ellipsoid *els, int nels)
{
	for (int i = 0; i < nels; i++)
		add_ellipsoid_if_visible(st, i);
	for (int i = 0; i < nwalls; i++)
		add_wall_if_visible(st, i);
	for (const int *i = highlightwalls; *i != -1; i++)
		st->infos[ID_NEW(ID_TYPE_WALL, *i)].highlight = true;
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

	add_visible_objects(&st, walls, nwalls, highlightwalls, els, nels);

	static ID objects_by_x[CAMERA_SCREEN_WIDTH][ARRAYLEN_CONTAINING_ID];
	int nobjects_by_x[CAMERA_SCREEN_WIDTH] = {0};

	for (int i = 0; i < st.nvisible; i++) {
		SDL_Rect bbox = st.infos[st.visible[i]].bbox;
		SDL_assert(0 <= bbox.x && bbox.x+bbox.w <= cam->surface->w);
		for (int x = bbox.x; x < bbox.x+bbox.w; x++)
			objects_by_x[x][nobjects_by_x[x]++] = st.visible[i];
	}

	setup_dependencies(&st, &objects_by_x, nobjects_by_x);

	static ID sorted[ArrayLen(st.visible)];
	create_sorted_array(&st, sorted);

	// static to keep stack usage down
	static ID objects_by_y[CAMERA_SCREEN_HEIGHT][ARRAYLEN_CONTAINING_ID];
	int nobjects_by_y[CAMERA_SCREEN_HEIGHT] = {0};

	for (int i = 0; i < st.nvisible; i++) {
		SDL_Rect bbox = st.infos[sorted[i]].bbox;
		SDL_assert(0 <= bbox.y && bbox.y+bbox.h <= cam->surface->h);
		for (int y = bbox.y; y < bbox.y+bbox.h; y++)
			objects_by_y[y][nobjects_by_y[y]++] = sorted[i];
	}

	for (int y = 0; y < cam->surface->h; y++) {
		static struct Interval intervals[ArrayLen(sorted)];
		int nintervals = 0;

		for (int i = 0; i < nobjects_by_y[y]; i++) {
			ID id = objects_by_y[y][i];
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
