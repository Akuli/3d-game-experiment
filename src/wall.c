#include "wall.h"
#include <math.h>
#include <stdint.h>
#include <SDL2/SDL.h>
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

	w->top1 = w->top2 = (Vec3){ (float)w->startx, Y_MAX, (float)w->startz };
	w->bot1 = w->bot2 = (Vec3){ (float)w->startx, Y_MIN, (float)w->startz };

	switch(w->dir) {
		case WALL_DIR_XY:
			w->top2.x += 1.0f;
			w->bot2.x += 1.0f;
			break;
		case WALL_DIR_ZY:
			w->top2.z += 1.0f;
			w->bot2.z += 1.0f;
			break;
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
		<=	diam(w)/2       + max(xzradius, yradius)   (because p is in ellipsoid)

	If this is not the case, then we can't have any intersections. We use
	this to optimize a common case.
	*/
	float diam = hypotf(Y_MAX - Y_MIN, 1);
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

static bool wall_is_visible(const struct Wall *w, const struct Camera *cam)
{
	Vec3 corners[] = { w->top1, w->top2, w->bot1, w->bot2 };

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

static void fill_cache(const struct Wall *w, struct WallCache *wc, const struct Camera *cam)
{
	wc->cam = cam;

	float dSQUARED = vec3_lengthSQUARED(vec3_sub(cam->location, wall_center(w)));
	wc->color = SDL_MapRGB(cam->surface->format, (int)( 0xaa*expf(-dSQUARED / 300.f) ), 0, 0);

	wc->top1 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->top1));
	wc->top2 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->top2));
	wc->bot1 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->bot1));
	wc->bot2 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->bot2));
}

bool wall_visible_xminmax(
	const struct Wall *w, const struct Camera *cam, int *xmin, int *xmax, struct WallCache *wc)
{
	if (!wall_is_visible(w, cam))
		return false;

	fill_cache(w, wc, cam);

	SDL_assert(fabsf(wc->top1.x - wc->bot1.x) < 1e-5f);
	SDL_assert(fabsf(wc->top2.x - wc->bot2.x) < 1e-5f);

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

	if (*ymin < 0)
		*ymin = 0;
	if (*ymax >= wc->cam->surface->h)
		*ymax = wc->cam->surface->h - 1;
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

inline bool wall_linedup(const struct Wall *w1, const struct Wall *w2);

// My simple way to add transparency is average of RGB colors. Works well enough for my needs.
static inline unsigned char bigger_value(unsigned char val) { return val + (0xff - val)/2; }
static inline unsigned char smoler_value(unsigned char val) { return val/2; }

void wall_drawcolumn(const struct WallCache *wc, int x, int ymin, int ymax)
{
	SDL_Surface *surf = wc->cam->surface;
	const SDL_PixelFormat *fmt = wc->cam->surface->format;

	SDL_assert(surf->pitch % sizeof(uint32_t) == 0);
	int mypitch = surf->pitch / sizeof(uint32_t);

	uint32_t *start = (uint32_t *)surf->pixels + ymin*mypitch + x;
	uint32_t *end   = (uint32_t *)surf->pixels + ymax*mypitch + x;
	for (uint32_t *ptr = start; ptr < end; ptr += mypitch) {
		// change between bigger_value and smoler_value to change wall color
		*ptr =
			(smoler_value((*ptr >> fmt->Rshift) & 0xff) << fmt->Rshift) |
			(bigger_value((*ptr >> fmt->Gshift) & 0xff) << fmt->Gshift) |
			(bigger_value((*ptr >> fmt->Bshift) & 0xff) << fmt->Bshift) |
			(*ptr & fmt->Amask);
	}
}
