#include "showall.h"
#include <assert.h>
#include <math.h>
#include "player.h"


struct GameObj {
	enum { ELLIPSOID, WALL } kind;
	union { const struct Ellipsoid *ellipsoid; const struct Wall *wall; } ptr;

	// all "dependency" GameObjs are drawn to screen first
	struct GameObj *deps[SHOWALL_MAX_OBJECTS];
	size_t ndeps;

	float centerz;   // z coordinate of center in camera coordinates
	bool shown;
};

static int compare_gameobj_ptrs_by_centerz(const void *aptr, const void *bptr)
{
	float az = (*(const struct GameObj **)aptr)->centerz;
	float bz = (*(const struct GameObj **)bptr)->centerz;
	return (az > bz) - (az < bz);
}

// Make sure that dep is shown before obj
// TODO: can be slow with many gameobjs, does it help to keep array always sorted and binsearch?
static void add_dependency(struct GameObj *obj, struct GameObj *dep)
{
	for (size_t i = 0; i < obj->ndeps; i++)
		if (obj->deps[i] == dep)
			return;
	obj->deps[obj->ndeps++] = dep;
}

static void figure_out_deps_for_ellipsoids_behind_walls(
	struct GameObj *walls, size_t nwalls,
	struct GameObj *els, size_t nels,
	const struct Camera *cam)
{
	for (size_t e = 0; e < nels; e++) {
		Vec3 center = els[e].ptr.ellipsoid->center;

		for (size_t w = 0; w < nwalls; w++) {
			/*
			Avoid funny issues near ends of the wall. Here 0.5 is a
			constant found with trial and error, so 0.5*radius is
			not a failed attempt at converting between a radius and
			a diameter.
			*/
			if (!wall_aligned_with_point(walls[w].ptr.wall, center, 0.5f * els[e].ptr.ellipsoid->xzradius))
				continue;

			bool camside = wall_side(walls[w].ptr.wall, cam->location);
			if (wall_side(walls[w].ptr.wall, center) == camside)
				add_dependency(&els[e], &walls[w]);
			else
				add_dependency(&walls[w], &els[e]);
		}
	}
}

static void add_wall_if_visible(
	struct GameObj *arr, size_t *len, const struct Wall *w, const struct Camera *cam)
{
	if (wall_visible(w, cam)) {
		arr[(*len)++] = (struct GameObj){
			.kind = WALL,
			.ptr = { .wall = w },
			.centerz = camera_point_world2cam(cam, wall_center(w)).z,
			0,    // rest zeroed, measurably faster than memset
		};
	}
}

static void add_ellipsoid_if_visible(
	struct GameObj *arr, size_t *len, const struct Ellipsoid *el, const struct Camera *cam)
{
	if (ellipsoid_visible(el, cam)) {
		arr[(*len)++] = (struct GameObj) {
			.kind = ELLIPSOID,
			.ptr = { .ellipsoid = el },
			.centerz = camera_point_world2cam(cam, el->center).z,
			0,
		};
	}
}

static struct GameObj **get_sorted_gameobj_pointers(
	struct GameObj *walls, size_t nwalls,
	struct GameObj *els, size_t nels)
{
	static struct GameObj *res[SHOWALL_MAX_OBJECTS];
	struct GameObj **ptr = res;
	for (size_t i = 0; i < nels; i++)
		*ptr++ = &els[i];
	for (size_t i = 0; i < nwalls; i++)
		*ptr++ = &walls[i];

	qsort(res, (size_t)(ptr - res), sizeof(res[0]), compare_gameobj_ptrs_by_centerz);
	return res;
}

static void show(struct GameObj *gobj, const struct Camera *cam, unsigned int depth)
{
	if (gobj->shown)
		return;

	if (depth > SHOWALL_MAX_OBJECTS) {
		// don't know when this could happen
		log_printf("hitting recursion depth limit");
		return;
	}

	/*
	prevent infinite recursion on dependency cycle by setting this early.
	I don't know what causes dependency cycles, but they happen sometimes.
	*/
	gobj->shown = true;

	for (size_t i = 0; i < gobj->ndeps; i++)
		show(gobj->deps[i], cam, depth+1);

	switch(gobj->kind) {
	case ELLIPSOID:
		ellipsoid_show(gobj->ptr.ellipsoid, cam);
		break;
	case WALL:
		wall_show(gobj->ptr.wall, cam);
		break;
	}
}

void show_all(
	const struct Wall *walls, size_t nwalls,
	const struct Player *plrs, size_t nplrs,
	const struct Enemy *ens, size_t nens,
	const struct Camera *cam)
{
	assert(nwalls <= PLACE_MAX_WALLS);
	assert(nplrs <= SHOWALL_MAX_PLAYERS);

	// these are static to keep stack usage down
	static struct GameObj wallobjs[PLACE_MAX_WALLS], elobjs[SHOWALL_MAX_ELLIPSOIDS];
	size_t nwallobjs = 0, nelobjs = 0;

	for (size_t i = 0; i < nwalls; i++)
		add_wall_if_visible(wallobjs, &nwallobjs, &walls[i], cam);
	for (size_t i = 0; i < nplrs; i++)
		add_ellipsoid_if_visible(elobjs, &nelobjs, &plrs[i].ellipsoid, cam);
	for (size_t i = 0; i < nens; i++)
		add_ellipsoid_if_visible(elobjs, &nelobjs, &ens[i].ellipsoid, cam);

	figure_out_deps_for_ellipsoids_behind_walls(wallobjs, nwallobjs, elobjs, nelobjs, cam);
	struct GameObj **sorted = get_sorted_gameobj_pointers(wallobjs, nwallobjs, elobjs, nelobjs);

	for (size_t i = 0; i < nwallobjs + nelobjs; i++)
		show(sorted[i], cam, 0);
}
