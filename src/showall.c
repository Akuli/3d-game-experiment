#include "showall.h"
#include <assert.h>


struct GameObj {
	enum GameObjKind { BALL, WALL } kind;
	union GameObjPtr { struct Ball *ball; const struct Wall *wall; } ptr;

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

static void figure_out_deps_for_balls_behind_walls(
	struct GameObj *walls, size_t nwalls,
	struct GameObj *balls, size_t nballs,
	struct Camera *cam)
{
	for (size_t w = 0; w < nwalls; w++) {
		bool camside = wall_side(walls[w].ptr.wall, cam->location);

		for (size_t b = 0; b < nballs; b++) {
			if (wall_side(walls[w].ptr.wall, balls[b].ptr.ball->center) == camside)
				add_dependency(&balls[b], &walls[w]);
			else
				add_dependency(&walls[w], &balls[b]);
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
		arr[*len].kind = kind;
		arr[*len].ptr = ptr;
		arr[*len].centerz = centerz;
		(*len)++;
	}
}

static void get_sorted_gameobj_pointers(
	struct GameObj *walls, size_t nwalls,
	struct GameObj *balls, size_t nballs,
	struct GameObj **res)
{
	for (size_t i = 0; i < nwalls; i++)
		res[i] = &walls[i];
	for (size_t i = 0; i < nballs; i++)
		res[nwalls + i] = &balls[i];
	qsort(res, nwalls + nballs, sizeof(res[0]), compare_gameobj_ptrs_by_centerz);
}

static void show(struct GameObj *gobj, struct Camera *cam, unsigned int depth)
{
	if (gobj->shown)
		return;

	/*
	Without the sanity check, this recurses infinitely when there is a dependency
	cycle. I don't know whether it's possible to create one, and I want to avoid
	random crashes.
	*/
	if (depth <= SHOWALL_MAX_OBJECTS) {
		for (size_t i = 0; i < gobj->ndeps; i++)
			show(gobj->deps[i], cam, depth+1);
	} else {
		nonfatal_error("hitting recursion depth limit");
	}

	switch(gobj->kind) {
	case BALL:
		ball_display(gobj->ptr.ball, cam);
		break;
	case WALL:
		wall_show(gobj->ptr.wall, cam);
		break;
	}
	gobj->shown = true;
}

void show_all(
	const struct Wall *walls, size_t nwalls,
	      struct Ball **balls, size_t nballs,
	struct Camera *cam)
{
	assert(nwalls <= SHOWALL_MAX_WALLS);
	assert(nballs <= SHOWALL_MAX_BALLS);

	// these are static to keep stack usage down
	static struct GameObj wallobjs[SHOWALL_MAX_WALLS], ballobjs[SHOWALL_MAX_BALLS];

	memset(wallobjs, 0, sizeof(wallobjs));
	memset(ballobjs, 0, sizeof(ballobjs));
	size_t nwallobjs = 0, nballobjs = 0;

	for (size_t i = 0; i < nwalls; i++)
		add_gameobj_to_array_if_visible(
			wallobjs, &nwallobjs, cam,
			wall_center(&walls[i]), WALL, (union GameObjPtr){ .wall = &walls[i] });

	for (size_t i = 0; i < nballs; i++)
		add_gameobj_to_array_if_visible(
			ballobjs, &nballobjs, cam,
			balls[i]->center, BALL, (union GameObjPtr){ .ball = balls[i] });

	struct GameObj *sorted[SHOWALL_MAX_OBJECTS];
	figure_out_deps_for_balls_behind_walls(wallobjs, nwallobjs, ballobjs, nballobjs, cam);
	get_sorted_gameobj_pointers(wallobjs, nwallobjs, ballobjs, nballobjs, sorted);

	for (size_t i = 0; i < nwallobjs + nballobjs; i++)
		show(sorted[i], cam, 0);
}
