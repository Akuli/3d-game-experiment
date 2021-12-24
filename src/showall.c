#include "showall.h"
#include <stdbool.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "ellipsoid.h"
#include "interval.h"
#include "log.h"
#include "max.h"
#include "misc.h"
#include "rect3.h"

// fitting too much stuff into an integer
typedef unsigned short ID;
#define ID_TYPE_ELLIPSOID 0
#define ID_TYPE_RECT 1
#define ID_TYPE(id) ((id) & 1)
#define ID_INDEX(id) ((id) >> 1)
#define ID_NEW(type, idx) ((type) | ((idx) << 1))

// length of array indexed by id
#define ARRAYLEN_INDEXED_BY_ID max( \
	ID_NEW(ID_TYPE_ELLIPSOID, MAX_ELLIPSOIDS-1) + 1, \
	ID_NEW(ID_TYPE_RECT, MAX_RECTS-1) + 1 \
)

// length of array containing ids
#define ARRAYLEN_CONTAINING_ID (MAX_ELLIPSOIDS + MAX_RECTS)

struct Info {
	// dependencies must be displayed first, they go to behind the ellipsoid or rect
	ID deps[ARRAYLEN_CONTAINING_ID];
	int ndeps;

	SDL_Rect bbox;	// bounding box
	struct Rect3 sortrect;
	bool sortingdone;  // for sorting infos to display them in correct order

	struct Rect3Cache rcache;  // ID_TYPE_RECT only
};

struct ShowingState {
	const struct Camera *cam;
	const struct Rect3 *rects;           // indexed by ID_INDEX(rect id)
	const struct Ellipsoid *els;        // indexed by ID_INDEX(ellipsoid id)
	struct Info infos[ARRAYLEN_INDEXED_BY_ID];

	ID visible[ARRAYLEN_CONTAINING_ID];
	int nvisible;

	// Visible objects in the order in which they are drawn (closest to camera last)
	ID objects_by_y[CAMERA_SCREEN_HEIGHT][ARRAYLEN_CONTAINING_ID];
	int nobjects_by_y[CAMERA_SCREEN_HEIGHT];
};

static void add_ellipsoid_if_visible(struct ShowingState *st, int idx)
{
	if (ellipsoid_is_visible(&st->els[idx], st->cam)) {
		ID id = ID_NEW(ID_TYPE_ELLIPSOID, idx);
		st->visible[st->nvisible++] = id;
		st->infos[id].ndeps = 0;
		st->infos[id].bbox = ellipsoid_bbox(&st->els[idx], st->cam);
		st->infos[id].sortrect = ellipsoid_get_sort_rect(&st->els[idx], st->cam);
		st->infos[id].sortingdone = false;
	}
}

static void add_rect_if_visible(struct ShowingState *st, int idx)
{
	struct Rect3Cache rcache;
	if (rect3_visible_fillcache(&st->rects[idx], st->cam, &rcache)) {
		ID id = ID_NEW(ID_TYPE_RECT, idx);
		st->visible[st->nvisible++] = id;
		st->infos[id].ndeps = 0;
		st->infos[id].bbox = rcache.bbox;
		st->infos[id].sortrect = st->rects[idx];
		st->infos[id].sortingdone = false;
		st->infos[id].rcache = rcache;
	}
}

// Debugging hint: rect3_drawborder
static void add_dependency(struct ShowingState *st, ID before, ID after)
{
	for (int i = 0; i < st->infos[after].ndeps; i++) {
		if (st->infos[after].deps[i] == before)
			return;
	}
	st->infos[after].deps[st->infos[after].ndeps++] = before;
}

// Return value: +-1 = all points on pos/neg side, 0 = points on different sides or all almost on plane
static int side_of_all_four_points(const struct Plane *pl, const Vec3 *points)
{
	int res = 0;
	for (int i=0; i<4; i++) {
		float d = pl->constant - vec3_dot(pl->normal, points[i]);
		if (fabsf(d) < 1e-5f)
			continue;
		if (d > 0) {
			if (res == -1)
				return 0;
			res = 1;
		} else {
			if (res == 1)
				return 0;
			res = -1;
		}
	}
	return res;
}

static void setup_dependencies(struct ShowingState *st)
{
	static struct Plane planes[sizeof st->visible / sizeof st->visible[0]];  // in camera coordinates
	static Vec3 camcorners[sizeof st->visible / sizeof st->visible[0]][4];  // in camera coordinates

	for (int i = 0; i < st->nvisible; i++) {
		/*SDL_Rect bbox = st.infos[st.visible[i]].bbox;
		SDL_assert(0 <= bbox.x && bbox.x+bbox.w <= cam->surface->w);
		*/
		const Vec3 *corners = st->infos[st->visible[i]].sortrect.corners;
		Vec3 n = vec3_cross(vec3_sub(corners[0], corners[1]), vec3_sub(corners[2], corners[1]));
		struct Plane pl = { .normal = n, .constant = vec3_dot(n, corners[0]) };
		plane_move(&pl, vec3_mul_float(st->cam->location, -1));
		plane_apply_mat3_INVERSE(&pl, st->cam->cam2world);

		// Make sure that camera (0,0,0) is on the positive side of the plane
		if (pl.constant < 0) {
			pl.constant *= -1;
			pl.normal.x *= -1;
			pl.normal.y *= -1;
			pl.normal.z *= -1;
		}
		planes[i] = pl;

		for (int k=0; k<4; k++)
			camcorners[i][k] = camera_point_world2cam(st->cam, corners[k]);
	}

	for (int i = 0; i < st->nvisible; i++) {
		const struct Info *iinfo = &st->infos[st->visible[i]];
		SDL_assert(0 <= iinfo->bbox.x && iinfo->bbox.x+iinfo->bbox.w <= st->cam->surface->w);

		for (int k = 0; k < i; k++) {
			const struct Info *kinfo = &st->infos[st->visible[k]];

			// Do not add dependencies between two walls
			if (ID_TYPE(st->visible[i]) == ID_TYPE_RECT
				&& ID_TYPE(st->visible[k]) == ID_TYPE_RECT
				&& iinfo->rcache.rect->img == NULL
				&& kinfo->rcache.rect->img == NULL
				&& iinfo->rcache.rect->highlight == kinfo->rcache.rect->highlight)
			{
				continue;
			}

			int xstart = max(iinfo->bbox.x, kinfo->bbox.x);
			int xend = min(iinfo->bbox.x + iinfo->bbox.w, kinfo->bbox.x + kinfo->bbox.w);
			if (xstart > xend)
				continue;

			int ystart = max(iinfo->bbox.y, kinfo->bbox.y);
			int yend = min(iinfo->bbox.y + iinfo->bbox.h, kinfo->bbox.y + kinfo->bbox.h);
			if (ystart > yend)
				continue;

			int s1 = side_of_all_four_points(&planes[i], camcorners[k]);
			int s2 = side_of_all_four_points(&planes[k], camcorners[i]);
			if (s1 == s2 && s1 != 0) {
				/*
				Both walls think they are on same/different side of the other wall as camera.
				Example of when this happens:

					 /  \
					/    \

					 cam

				Avoid dependency cycle, order doesn't seem to matter.
				*/
				continue;
			}

			if (s1 == -1 || s2 == 1)
				add_dependency(st, st->visible[k], st->visible[i]);
			if (s1 == 1 || s2 == -1)
				add_dependency(st, st->visible[i], st->visible[k]);
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

static void break_dependency_cycle(struct ShowingState *st, ID start)
{
	/*
	Consider the sequence (x_n), where x_1 = start and x_(n+1) = st->infos[x_n].deps[0].
	If all elements have some dependencies, this is an infinite sequence of finitely many
	elements to choose from, so it will eventually cycle. To find a cycle, we compare x_n
	and x_(2n) until they match.
	*/
#define next(x) st->infos[x].deps[0]
	ID x = start;
	ID y = next(start);
	while (x != y) {
		x = next(x);
		y = next(next(y));
	}
#undef next

	st->infos[x].deps[0] = st->infos[x].deps[--st->infos[x].ndeps];
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
			break_dependency_cycle(st, todo[0]);
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
		case ID_TYPE_RECT: return rect3_xminmax(&st->infos[id].rcache, y, xmin, xmax);
	}
	return false;  // compiler = happy
}

static void draw_row(const struct ShowingState *st, int y, ID id, int xmin, int xmax)
{
	switch(ID_TYPE(id)) {
	case ID_TYPE_ELLIPSOID:
		ellipsoid_drawrow(&st->els[ID_INDEX(id)], st->cam, y, xmin, xmax);
		break;
	case ID_TYPE_RECT:
		rect3_drawrow(&st->infos[id].rcache, y, xmin, xmax);
		break;
	}
}

void show_all(
	const struct Rect3 *rects, int nrects,
	const struct Ellipsoid *els, int nels,
	const struct Camera *cam)
{
	SDL_assert(nrects <= MAX_RECTS);
	SDL_assert(nels <= MAX_ELLIPSOIDS);

	// static to keep stack usage down
	static struct ShowingState st;
	st.cam = cam;
	st.rects = rects;
	st.els = els;
	st.nvisible = 0;
	memset(st.nobjects_by_y, 0, sizeof st.nobjects_by_y);

	for (int i = 0; i < nels; i++)
		add_ellipsoid_if_visible(&st, i);
	for (int i = 0; i < nrects; i++)
		add_rect_if_visible(&st, i);

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
					.allowoverlap = (ID_TYPE(id) == ID_TYPE_RECT),
				};
			}
		}

		static struct Interval nonoverlap[INTERVAL_NON_OVERLAPPING_MAX(ARRAYLEN_CONTAINING_ID)];
		int nnonoverlap = interval_non_overlapping(intervals, nintervals, nonoverlap);

		for (int i = 0; i < nnonoverlap; i++)
			draw_row(&st, y, nonoverlap[i].id, nonoverlap[i].start, nonoverlap[i].end);
	}
}
