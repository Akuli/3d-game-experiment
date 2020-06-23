#include "showall.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include "camera.h"
#include "ellipsoid.h"
#include "interval.h"
#include "player.h"
#include "mathstuff.h"

// fitting too much stuff into an int
typedef int ID;
#define ID_TYPE_ELLIPSOID 0
#define ID_TYPE_WALL 1
#define ID_TYPE(id) ((id) & 1)
#define ID_INDEX(id) ((id) >> 1)
#define ID_NEW(type, idx) ((type) | ((idx) << 1))

// length of array indexed by id
#define ID_ARRAYLEN max( \
	ID_NEW(ID_TYPE_ELLIPSOID, SHOWALL_MAX_ELLIPSOIDS-1) + 1, \
	ID_NEW(ID_TYPE_WALL, PLACE_MAX_WALLS-1) + 1 \
)

struct Info {
	// dependencies must be displayed first, they go to behind the ellipsoid or wall
	ID deps[PLACE_MAX_WALLS + SHOWALL_MAX_ELLIPSOIDS];
	int ndeps;

	// which range of x coordinates will be showing this ellipsoid or wall?
	int xmin, xmax;

	// for sorting infos to display them in correct order
	bool insortedarray;

	union {
		struct WallCache wallc;
		struct EllipsoidXCache ellipsoidc;
	} cache;
};

struct ShowingState {
	const struct Camera *cam;
	const struct Wall *walls;           // indexed by ID_INDEX(wall id)
	const struct Ellipsoid *els;        // indexed by ID_INDEX(ellipsoid id)
	struct Info infos[ID_ARRAYLEN];     // indexed by id

	ID visible[PLACE_MAX_WALLS + SHOWALL_MAX_ELLIPSOIDS];
	int nvisible;
};

static void add_ellipsoid_if_visible(struct ShowingState *st, int idx)
{
	int xmin, xmax;
	if (ellipsoid_visible_xminmax(&st->els[idx], st->cam, &xmin, &xmax)) {
		ID id = ID_NEW(ID_TYPE_ELLIPSOID, idx);
		st->visible[st->nvisible++] = id;
		st->infos[id] = (struct Info) {
			.ndeps = 0,
			.xmin = xmin,
			.xmax = xmax,
			.insortedarray = false,
		};
	}
}

static void add_wall_if_visible(struct ShowingState *st, int idx)
{
	int xmin, xmax;
	struct WallCache wc;
	if (wall_visible_xminmax(&st->walls[idx], st->cam, &xmin, &xmax, &wc)) {
		ID id = ID_NEW(ID_TYPE_WALL, idx);
		st->visible[st->nvisible++] = id;
		st->infos[id] = (struct Info){
			.ndeps = 0,
			.xmin = xmin,
			.xmax = xmax,
			.insortedarray = false,
			.cache = { .wallc = wc },
		};
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

static void setup_ellipsoid_wall_dependency(struct ShowingState *st, ID eid, ID wid)
{
	assert(ID_TYPE(eid) == ID_TYPE_ELLIPSOID);
	assert(ID_TYPE(wid) == ID_TYPE_WALL);

	const struct Ellipsoid *el = &st->els[ID_INDEX(eid)];
	const struct Wall *w = &st->walls[ID_INDEX(wid)];
	if (wall_side(w, el->center) == wall_side(w, st->cam->location))
		add_dependency(st, wid, eid);
	else
		add_dependency(st, eid, wid);
}

static void setup_same_type_dependency(struct ShowingState *st, ID id1, ID id2)
{
	assert(ID_TYPE(id1) == ID_TYPE(id2));
	Vec3 center1, center2;

	switch(ID_TYPE(id1)) {
		case ID_TYPE_ELLIPSOID:
			center1 = st->els[ID_INDEX(id1)].center;
			center2 = st->els[ID_INDEX(id2)].center;
			break;

		case ID_TYPE_WALL:
			if (wall_linedup(&st->walls[ID_INDEX(id1)], &st->walls[ID_INDEX(id2)]))
				return;
			center1 = wall_center(&st->walls[ID_INDEX(id1)]);
			center2 = wall_center(&st->walls[ID_INDEX(id2)]);
			break;

		default: return;    // make compiler happy
	}

	float c1, c2;
	if (center1.x == center2.x && center1.z == center2.z) {
		// comparing y coordinates does the right thing for guards above players
		c1 = center1.y;
		c2 = center2.y;
	} else {
		/*
		using distance between camera and center instead creates funny bug: Set up
		two players with wall in between them. Make player A look at player B
		(behind the wall), and jump up and down with player B. Sometimes B will
		show up through the wall.

		tl;dr: don't "improve" this code by replacing z coordinate (in camera coords)
		       with something like |camera - center|
		*/
		c1 = camera_point_world2cam(st->cam, center1).z;
		c2 = camera_point_world2cam(st->cam, center2).z;
	}

	if (c1 < c2)
		add_dependency(st, id1, id2);
	else
		add_dependency(st, id2, id1);
}

static void setup_dependencies(struct ShowingState *st)
{
	for (int i = 0  ; i < st->nvisible; i++) {
		for (int k = i+1; k < st->nvisible; k++) {
			ID id1 = st->visible[i];
			ID id2 = st->visible[k];
			if (!interval_overlap(
					st->infos[id1].xmin, st->infos[id1].xmax,
					st->infos[id2].xmin, st->infos[id2].xmax))
			{
				continue;
			}

			if (ID_TYPE(id1) == ID_TYPE_ELLIPSOID && ID_TYPE(id2) == ID_TYPE_WALL)
				setup_ellipsoid_wall_dependency(st, id1, id2);
			else if (ID_TYPE(id1) == ID_TYPE_WALL && ID_TYPE(id2) == ID_TYPE_ELLIPSOID)
				setup_ellipsoid_wall_dependency(st, id2, id1);
			else
				setup_same_type_dependency(st, id1, id2);
		}
	}
}

#if 0
static void debug_print_dependencies(const struct ShowingState *st)
{
	printf("\ndependency dump for camera %s:\n", st->cam->id);
	for (int i = 0; i < st->nvisible; i++) {
		ID id = st->visible[i];
		printf("   %-3d --> ", id);
		for (int d = 0; d < st->infos[id].ndeps; d++)
			printf("%s%d", (d?",":""), st->infos[id].deps[d]);
		printf("\n");
	}
	printf("\n");
}
#endif

static void add_dependencies_and_id_to_sorted_array(struct ShowingState *st, ID **ptr, ID id)
{
	if (st->infos[id].insortedarray)
		return;

	// setting this early avoids infinite recursion in case dependency graph has a cycle
	st->infos[id].insortedarray = true;   

	for (int i = 0; i < st->infos[id].ndeps; i++)
		add_dependencies_and_id_to_sorted_array(st, ptr, st->infos[id].deps[i]);

	*(*ptr)++ = id;
}

// In which order should walls and ellipsoids be shown?
static void create_sorted_array(struct ShowingState *st, ID *sorted)
{
	ID *ptr = sorted;
	for (int i = 0; i < st->nvisible; i++)
		add_dependencies_and_id_to_sorted_array(st, &ptr, st->visible[i]);
	assert(sorted + st->nvisible == ptr);
}

static void get_yminmax(struct ShowingState *st, ID id, int x, int *ymin, int *ymax)
{
	switch(ID_TYPE(id)) {
		case ID_TYPE_ELLIPSOID:
			ellipsoid_yminmax(&st->els[ID_INDEX(id)], st->cam, x, &st->infos[id].cache.ellipsoidc, ymin, ymax);
			break;
		case ID_TYPE_WALL:
			wall_yminmax(&st->infos[id].cache.wallc, x, ymin, ymax);
			break;
	}
}

static void draw_column(const struct ShowingState *st, int x, ID id, int ymin, int ymax)
{
	float dSQUARED;

	switch(ID_TYPE(id)) {
	case ID_TYPE_ELLIPSOID:
		ellipsoid_drawcolumn(&st->els[ID_INDEX(id)], &st->infos[id].cache.ellipsoidc, ymin, ymax);
		break;
	case ID_TYPE_WALL:
		dSQUARED = vec3_lengthSQUARED(vec3_sub(st->cam->location, wall_center(&st->walls[ID_INDEX(id)])));
		SDL_FillRect(
			st->cam->surface,
			&(SDL_Rect){x, ymin, 1, ymax-ymin},
			SDL_MapRGB(st->cam->surface->format, (int)( 0xaa*expf(-dSQUARED / 300.f) ), 0, 0));
		break;
	}
}

#define ArrayLen(arr) ( sizeof(arr) / sizeof((arr)[0]) )

void show_all(
	const struct Wall *walls, int nwalls,
	const struct Ellipsoid *els, int nels,
	const struct Camera *cam)
{
	assert(nwalls <= PLACE_MAX_WALLS);
	assert(nels <= SHOWALL_MAX_ELLIPSOIDS);

	// static to keep stack usage down
	static struct ShowingState st;
	st.cam = cam;
	st.walls = walls;
	st.els = els;
	st.nvisible = 0;

	for (int i = 0; i < nwalls; i++)
		add_wall_if_visible(&st, i);
	for (int i = 0; i < nels; i++)
		add_ellipsoid_if_visible(&st, i);

	setup_dependencies(&st);

	static ID sorted[ArrayLen(st.visible)];
	create_sorted_array(&st, sorted);

	for (int x = 0; x < cam->surface->w; x++) {
		static struct Interval intervals[ArrayLen(sorted)];
		int nintervals = 0;

		for (int i = 0; i < st.nvisible; i++) {
			ID id = sorted[i];
			if (!( st.infos[id].xmin <= x && x <= st.infos[id].xmax ))
				continue;

			int ymin, ymax;
			get_yminmax(&st, id, x, &ymin, &ymax);
			if (ymin < ymax) {
				intervals[nintervals++] = (struct Interval){
					.start = ymin,
					.end = ymax,
					.id = id,
				};
			}
		}

		static struct Interval nonoverlap[INTERVAL_NON_OVERLAPPING_MAX(ArrayLen(intervals))];
		int nnonoverlap = interval_non_overlapping(intervals, nintervals, nonoverlap);

		for (int i = 0; i < nnonoverlap; i++)
			draw_column(&st, x, nonoverlap[i].id, nonoverlap[i].start, nonoverlap[i].end);
	}
}
