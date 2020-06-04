#include "showall.h"
#include <assert.h>
#include <math.h>


struct GameObj {
	enum GameObjKind { BALL, WALL } kind;
	union GameObjPtr { struct Ellipsoid *ellipsoid; const struct Wall *wall; } ptr;

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
	struct Camera *cam)
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

static void add_gameobj_to_array_if_visible(
	struct GameObj *arr, size_t *len,
	const struct Camera *cam,
	Vec3 center, enum GameObjKind kind, union GameObjPtr ptr)
{
	float centerz = camera_point_world2cam(cam, center).z;
	if (centerz < 0) {   // in front of camera
		arr[(*len)++] = (struct GameObj){
			.kind = kind,
			.ptr = ptr,
			.centerz = centerz,
			0,   // rest zeroed, measurably faster than memset
		};
	}
}

static struct GameObj **get_sorted_gameobj_pointers(
	struct GameObj *walls, size_t nwalls,
	struct GameObj *els, size_t nels)
{
	static struct GameObj *res[SHOWALL_MAX_OBJECTS];
	for (size_t i = 0; i < nwalls; i++)
		res[i] = &walls[i];
	for (size_t i = 0; i < nels; i++)
		res[nwalls + i] = &els[i];
	qsort(res, nwalls + nels, sizeof(res[0]), compare_gameobj_ptrs_by_centerz);
	return res;
}

static void show(struct GameObj *gobj, struct Camera *cam, unsigned int depth)
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
	case BALL:
		ellipsoid_show(gobj->ptr.ellipsoid, cam);
		break;
	case WALL:
		wall_show(gobj->ptr.wall, cam);
		break;
	}
}

void show_all(
	const struct Wall *walls, size_t nwalls,
	      struct Ellipsoid **els, size_t nels,
	struct Camera *cam)
{
	assert(nwalls <= PLACE_MAX_WALLS);
	assert(nels <= SHOWALL_MAX_BALLS);

	// these are static to keep stack usage down
	static struct GameObj wallobjs[PLACE_MAX_WALLS], elobjs[SHOWALL_MAX_BALLS];
	size_t nwallobjs = 0, nelobjs = 0;

	for (size_t i = 0; i < nwalls; i++)
		add_gameobj_to_array_if_visible(
			wallobjs, &nwallobjs, cam,
			wall_center(&walls[i]), WALL, (union GameObjPtr){ .wall = &walls[i] });

	for (size_t i = 0; i < nels; i++)
		add_gameobj_to_array_if_visible(
			elobjs, &nelobjs, cam,
			els[i]->center, BALL, (union GameObjPtr){ .ellipsoid = els[i] });

	figure_out_deps_for_ellipsoids_behind_walls(wallobjs, nwallobjs, elobjs, nelobjs, cam);
	struct GameObj **sorted = get_sorted_gameobj_pointers(wallobjs, nwallobjs, elobjs, nelobjs);

	for (size_t i = 0; i < nwallobjs + nelobjs; i++)
		show(sorted[i], cam, 0);
}
