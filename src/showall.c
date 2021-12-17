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
	ID deps[ARRAYLEN_CONTAINING_ID];
	int ndeps;

	SDL_Rect bbox;	// bounding box
	struct Rect sortrect;
	bool sortingdone;  // for sorting infos to display them in correct order

	struct RectCache rcache;
	bool highlight;
};

struct ShowingState {
	const struct Camera *cam;
	const struct Wall *walls;	    // indexed by ID_INDEX(wall id)
	const struct Ellipsoid *els;	 // indexed by ID_INDEX(ellipsoid id)
	struct Info infos[ARRAYLEN_INDEXED_BY_ID];

	ID visible[ARRAYLEN_CONTAINING_ID];
	int nvisible;

	// Visible objects in arbitrary order
	ID objects_by_x[CAMERA_SCREEN_WIDTH][ARRAYLEN_CONTAINING_ID];
	int nobjects_by_x[CAMERA_SCREEN_WIDTH];

	// Visible objects in the order in which they are drawn (bottommost first)
	ID objects_by_y[CAMERA_SCREEN_HEIGHT][ARRAYLEN_CONTAINING_ID];
	int nobjects_by_y[CAMERA_SCREEN_HEIGHT];
};

static void add_ellipsoid_if_visible(struct ShowingState *st, int idx)
{
	if (ellipsoid_is_visible(&st->els[idx], st->cam)) {
		ID id = ID_NEW(ID_TYPE_ELLIPSOID, idx);
		st->visible[st->nvisible++] = id;
		st->infos[id].ndeps = 0;
		st->infos[id].bbox = ellipsoid_bounding_box(&st->els[idx], st->cam);
		st->infos[id].sortrect = ellipsoid_get_sort_rect(&st->els[idx], st->cam);
		st->infos[id].sortingdone = false;
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
		st->infos[id].sortingdone = false;
		st->infos[id].rcache = rcache;
		st->infos[id].highlight = false;
	}
}

#if 0
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
#endif

static void add_dependency(struct ShowingState *st, ID before, ID after)
{
	for (int i = 0; i < st->infos[after].ndeps; i++) {
		if (st->infos[after].deps[i] == before)
			return;
	}
	st->infos[after].deps[st->infos[after].ndeps++] = before;
}

static void setup_dependencies(struct ShowingState *st)
{
	bool xcoords[CAMERA_SCREEN_WIDTH] = {0};
	for (int i = 0; i < st->nvisible; i++) {
		SDL_Rect bbox = st->infos[st->visible[i]].bbox;
		int lo = bbox.x, hi = bbox.x+bbox.w;
		SDL_assert(0 <= lo && lo <= CAMERA_SCREEN_WIDTH);
		SDL_assert(0 <= hi && hi <= CAMERA_SCREEN_WIDTH);
		if (lo != CAMERA_SCREEN_WIDTH) xcoords[lo] = true;
		if (hi != CAMERA_SCREEN_WIDTH) xcoords[hi] = true;
	}

	int xlist[CAMERA_SCREEN_WIDTH];
	int xlistlen = 0;
	for (int x = 0; x < sizeof(xcoords)/sizeof(xcoords[0]); x++)
		if (xcoords[x])
			xlist[xlistlen++] = x;

	// It should be enough to check in the middles of intervals between bboxes
	for (int xidx = 1; xidx < xlistlen; xidx++) {
		int x = (xlist[xidx-1] + xlist[xidx])/2;
		float xzr = camera_screenx_to_xzr(st->cam, x);

		for (int i = 0; i < st->nobjects_by_x[x]; i++)
			for (int k = 0; k < i; k++)
			{
				ID id1 = st->objects_by_x[x][i];
				ID id2 = st->objects_by_x[x][k];
				if (ID_TYPE(id1) == ID_TYPE_WALL && ID_TYPE(id2) == ID_TYPE_WALL)
					continue;

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
}

// Called for each visible object, in the order of drawing
static void add_id_to_drawing_order(struct ShowingState *st, ID id)
{
	SDL_Rect bbox = st->infos[id].bbox;
	SDL_assert(0 <= bbox.y && bbox.y+bbox.h <= st->cam->surface->h);
	for (int y = bbox.y; y < bbox.y+bbox.h; y++)
		st->objects_by_y[y][st->nobjects_by_y[y]++] = id;
}

static void create_showing_order_from_dependencies(struct ShowingState *st)
{
	static ID todo[ARRAYLEN_CONTAINING_ID];
	int ntodo = st->nvisible;
	memcpy(todo, st->visible, ntodo*sizeof(todo[0]));

	// Standard topological sort algorithm. I learned it from Python's toposort module
	while (ntodo > 0) {
		bool stuck = true;
		for (int i = ntodo-1; i >= 0; i--) {
			if (st->infos[todo[i]].ndeps == 0) {
				add_id_to_drawing_order(st, todo[i]);
				st->infos[todo[i]].sortingdone = true;
				todo[i] = todo[--ntodo];
				stuck = false;
			}
		}
		if (stuck) {
			log_printf("dependency cycle detected");
			st->infos[todo[0]].ndeps--;
			continue;
		}
		for (int i = 0; i < ntodo; i++) {
			struct Info *info = &st->infos[todo[i]];
			for (int k = info->ndeps-1; k >= 0; k--) {
				if (st->infos[info->deps[k]].sortingdone)
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
	memset(st.nobjects_by_x, 0, sizeof st.nobjects_by_x);
	memset(st.nobjects_by_y, 0, sizeof st.nobjects_by_y);

	for (int i = 0; i < nels; i++)
		add_ellipsoid_if_visible(&st, i);
	for (int i = 0; i < nwalls; i++)
		add_wall_if_visible(&st, i);
	for (const int *i = highlightwalls; *i != -1; i++)
		st.infos[ID_NEW(ID_TYPE_WALL, *i)].highlight = true;

	for (int i = 0; i < st.nvisible; i++) {
		SDL_Rect bbox = st.infos[st.visible[i]].bbox;
		SDL_assert(0 <= bbox.x && bbox.x+bbox.w <= cam->surface->w);
		for (int x = bbox.x; x < bbox.x+bbox.w; x++)
			st.objects_by_x[x][st.nobjects_by_x[x]++] = st.visible[i];
	}

	setup_dependencies(&st);
	create_showing_order_from_dependencies(&st);

	for (int y = 0; y < cam->surface->h; y++) {
		static struct Interval intervals[ARRAYLEN_CONTAINING_ID];
		int nintervals = 0;

		for (int i = 0; i < st.nobjects_by_y[y]; i++) {
			ID id = st.objects_by_y[y][i];
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

		static struct Interval nonoverlap[INTERVAL_NON_OVERLAPPING_MAX(ARRAYLEN_CONTAINING_ID)];
		int nnonoverlap = interval_non_overlapping(intervals, nintervals, nonoverlap);

		for (int i = 0; i < nnonoverlap; i++)
			draw_row(&st, y, nonoverlap[i].id, nonoverlap[i].start, nonoverlap[i].end);
	}
}
