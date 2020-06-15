#include "wall.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include "camera.h"
#include "mathstuff.h"
#include "player.h"

#define Y_MIN PLAYER_HEIGHT_FLAT   // allow players to go under the wall
#define Y_MAX 1.0f


static float linear_map(float srcmin, float srcmax, float dstmin, float dstmax, float val)
{
	float ratio = (val - srcmin)/(srcmax - srcmin);
	return dstmin + ratio*(dstmax - dstmin);
}

void wall_init(struct Wall *w)
{
	for (int xznum = 0; xznum < WALL_CP_COUNT; xznum++) {
		for (int ynum = 0; ynum < WALL_CP_COUNT; ynum++) {
			Vec3 *ptr = &w->collpoints[xznum][ynum];
			ptr->x = (float)w->startx;
			ptr->y = linear_map(0, WALL_CP_COUNT-1, Y_MIN, Y_MAX, (float)ynum);
			ptr->z = (float)w->startz;

			switch(w->dir) {
			case WALL_DIR_XY:
				ptr->x += linear_map(0, WALL_CP_COUNT-1, 0, 1, (float)xznum);
				break;
			case WALL_DIR_ZY:
				ptr->z += linear_map(0, WALL_CP_COUNT-1, 0, 1, (float)xznum);
				break;
			}
		}
	}
}

void wall_bumps_ellipsoid(const struct Wall *w, struct Ellipsoid *el)
{
	/*
	If the ellipsoid is very far away from wall, then it surely doesn't bump. We
	use this idea to optimize the common case. But how much is "very far away"?

	Suppose that the ellipsoid and wall intersect at some point p. Let c be
	the corner of the wall that is most far away from the ellipsoid. Let
	diam(w) denote the distance between opposite corners of a wall. Then

			|center(w) - center(el)|
		=	|center(w) - p  +  p - center(el)|         (because -p+p = zero vector)
		<=	|center(w) - p| + |p - center(el)|         (by triangle inequality)
		<=	diam(w)/2       + |p - center(el)|         (because p is in wall)
		<=	diam(w)/2       + max(xzradius, yradius)   (because p is in wall)

	If this is not the case, then we can't have any intersections. We use
	this to optimize a common case.
	*/
	float h = Y_MAX - Y_MIN;
	float diam = hypotf(h, 1);
	float thing = diam/2 + max(el->xzradius, el->yradius);
	if (vec3_lengthSQUARED(vec3_sub(el->center, wall_center(w))) > thing*thing)
		return;

	// Switch to coordinates where the ellipsoid is a ball with radius 1
	Vec3 elcenter = mat3_mul_vec3(el->transform_inverse, el->center);

	for (int xznum = 0; xznum < WALL_CP_COUNT; xznum++) {
		for (int ynum = 0; ynum < WALL_CP_COUNT; ynum++) {
			Vec3 collpoint = mat3_mul_vec3(el->transform_inverse, w->collpoints[xznum][ynum]);
			Vec3 diff = vec3_sub(elcenter, collpoint);

			float distSQUARED = vec3_lengthSQUARED(diff);
			if (distSQUARED >= 1)   // doesn't bump
				continue;

			float dist = sqrtf(distSQUARED);

			diff.y = 0;   // don't move up/down
			diff = vec3_withlength(diff, 1 - dist);  // move just enough to not touch
			vec3_apply_matrix(&diff, el->transform);

			// if we're not bumping on the edge of the wall
			if (xznum != 0 && xznum != WALL_CP_COUNT - 1) {
				// then we should move only in direction opposite to wall
				switch(w->dir) {
					case WALL_DIR_XY: diff.x = 0; break;
					case WALL_DIR_ZY: diff.z = 0; break;
				}
			}

			vec3_add_inplace(&el->center, diff);
			elcenter = mat3_mul_vec3(el->transform_inverse, el->center);   // cache invalidation
		}
	}
}

static void get_corners_in_world_coordinates(
	const struct Wall *w,
	Vec3 *top1, Vec3 *top2,
	Vec3 *bot1, Vec3 *bot2)
{
	*top1 = *top2 = (Vec3){ (float)w->startx, Y_MAX, (float)w->startz };
	*bot1 = *bot2 = (Vec3){ (float)w->startx, Y_MIN, (float)w->startz };

	switch(w->dir) {
		case WALL_DIR_XY:
			top2->x += 1.0f;
			bot2->x += 1.0f;
			break;
		case WALL_DIR_ZY:
			top2->z += 1.0f;
			bot2->z += 1.0f;
			break;
	}
}

static bool wall_is_visible(const Vec3 *corners, const struct Camera *cam)
{
	// Ensure that no corner is behind camera. This means that x/z ratios will work.
	for (int c = 0; c < 4; c++)
		if (!plane_whichside(cam->visplanes[CAMERA_CAMPLANE_IDX], corners[c]))
			return false;

	// check if any corner is visible
	for (int c = 0; c < 4; c++) {
		bool cornervisible = true;
		for (int v = 0; v < sizeof(cam->visplanes)/sizeof(cam->visplanes[0]); v++) {
			if (!plane_whichside(cam->visplanes[v], corners[c])) {
				cornervisible = false;
				break;
			}
		}

		if (cornervisible)
			return true;
	}
	return false;
}

bool wall_visible_xminmax(
	const struct Wall *w, const struct Camera *cam, int *xmin, int *xmax, struct WallCache *wc)
{
	Vec3 top1, top2, bot1, bot2;
	get_corners_in_world_coordinates(w, &top1, &top2, &bot1, &bot2);
	if (!wall_is_visible((const Vec3[]){ top1, top2, bot1, bot2 }, cam))
		return false;

	wc->top1 = camera_point_cam2screen(cam, camera_point_world2cam(cam, top1));
	wc->top2 = camera_point_cam2screen(cam, camera_point_world2cam(cam, top2));
	wc->bot1 = camera_point_cam2screen(cam, camera_point_world2cam(cam, bot1));
	wc->bot2 = camera_point_cam2screen(cam, camera_point_world2cam(cam, bot2));

	assert(fabsf(wc->top1.x - wc->bot1.x) < 1e-5f);
	assert(fabsf(wc->top2.x - wc->bot2.x) < 1e-5f);

	// need only top corners because others have same screen x
	*xmin = (int)ceilf(min(wc->top1.x, wc->top2.x));
	*xmax = (int)      max(wc->top1.x, wc->top2.x);
	return (*xmin <= *xmax);
}

void wall_yminmax(const struct WallCache *wc, int x, int *ymin, int *ymax)
{
	if (fabsf(wc->top1.x - wc->top2.x) < 1e-5f) {
		// would get issues in linear_map()
		*ymin = 0;
		*ymax = 0;
	}
	*ymin = (int) linear_map(wc->top1.x, wc->top2.x, wc->top1.y, wc->top2.y, x);
	*ymax = (int) linear_map(wc->bot1.x, wc->bot2.x, wc->bot1.y, wc->bot2.y, x);
}

Vec3 wall_center(const struct Wall *w)
{
	float x = (float)w->startx;
	float y = (Y_MIN + Y_MAX)/2;
	float z = (float)w->startz;

	switch(w->dir) {
		case WALL_DIR_XY: x += 0.5f; break;
		case WALL_DIR_ZY: z += 0.5f; break;
	}

	return (Vec3){x,y,z};
}

bool wall_aligned_with_point(const struct Wall *w, Vec3 pt, float offmax)
{
	switch(w->dir) {
		case WALL_DIR_XY: return (float)w->startx - offmax < pt.x && pt.x < (float)(w->startx + 1) + offmax;
		case WALL_DIR_ZY: return (float)w->startz - offmax < pt.z && pt.z < (float)(w->startz + 1) + offmax;
	}
	return false;   // never runs, make compiler happy
}

bool wall_side(const struct Wall *w, Vec3 pt)
{
	Vec3 center = wall_center(w);
	switch(w->dir) {
		case WALL_DIR_XY: return (center.z < pt.z);
		case WALL_DIR_ZY: return (center.x < pt.x);
	}

	// never actually runs, but makes compiler happy
	return false;
}

bool wall_linedup(const struct Wall *w1, const struct Wall *w2)
{
	if (w1->dir != w2->dir)
		return false;

	switch(w1->dir) {
		case WALL_DIR_XY: return (w1->startz == w2->startz);
		case WALL_DIR_ZY: return (w1->startx == w2->startx);
	}

	// never actually runs, makes compiler happy
	return false;
}
