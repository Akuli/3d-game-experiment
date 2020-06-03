#include "wall.h"
#include <assert.h>
#include <stdint.h>
#include "camera.h"
#include "mathstuff.h"
#include "player.h"
#include <SDL2/SDL2_gfxPrimitives.h>

#define Y_MIN PLAYER_HEIGHT_FLAT   // allow players to go under the wall
#define Y_MAX 1.0f


static float linear_map(float srcmin, float srcmax, float dstmin, float dstmax, float val)
{
	float ratio = (val - srcmin)/(srcmax - srcmin);
	return dstmin + ratio*(dstmax - dstmin);
}

static int linear_map_int(int srcmin, int srcmax, int dstmin, int dstmax, int val)
{
	return (int)linear_map((float)srcmin, (float)srcmax, (float)dstmin, (float)dstmax, (float)val);
}

void wall_initcaches(struct Wall *w)
{
	for (int xznum = 0; xznum < WALL_CP_COUNT; xznum++) {
		for (int ynum = 0; ynum < WALL_CP_COUNT; ynum++) {
			Vec3 *ptr = &w->collpoint_cache[xznum][ynum];
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
	// Switch to coordinates where the ellipsoid is a el with radius 1
	Vec3 center = mat3_mul_vec3(el->transform_inverse, el->center);

	for (int xznum = 0; xznum < WALL_CP_COUNT; xznum++) {
		for (int ynum = 0; ynum < WALL_CP_COUNT; ynum++) {
			Vec3 collpoint = mat3_mul_vec3(el->transform_inverse, w->collpoint_cache[xznum][ynum]);
			Vec3 diff = vec3_sub(center, collpoint);

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
			center = mat3_mul_vec3(el->transform_inverse, el->center);   // cache invalidation
		}
	}
}

static void get_corners_in_camera_coordinates(const struct Wall *w, const struct Camera *cam, Vec3 *res)
{
	// initial values never used, but they make compiler happy
	int endx = 0, endz = 0;

	switch(w->dir) {
	case WALL_DIR_XY:
		endx = w->startx + 1;
		endz = w->startz;
		break;
	case WALL_DIR_ZY:
		endx = w->startx;
		endz = w->startz + 1;
		break;
	}

#define f(x,y,z) camera_point_world2cam(cam, (Vec3){ (float)(x), (float)(y), (float)(z) })
	res[0] = f(w->startx, Y_MIN, w->startz);
	res[1] = f(w->startx, Y_MAX, w->startz);
	res[2] = f(endx,      Y_MIN, endz);
	res[3] = f(endx,      Y_MAX, endz);
#undef f
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

static int line_x(SDL_Point p1, SDL_Point p2, int y)
{
	if (p1.y == p2.y)
		return p1.x;   // avoid division by zero
	return linear_map_int(p1.y, p2.y, p1.x, p2.x, y);
}

/*
Transparent walls are implemented in a shitty way because I couldn't get SDL's
transparency stuff to work correctly. I can't use SDL_FillRect because it
doesn't support transparency.
*/
#define R_INCREMENT 100
#define G_INCREMENT 0
#define B_INCREMENT 0

#define USE_TRANSPARENCY 1

static void draw_hline(SDL_Surface *surf, int x1, int x2, int y)
{
#if USE_TRANSPARENCY
	if (y < 0 || y >= surf->h)
		return;

	if (x1 > x2) {
		int tmp = x1;
		x1 = x2;
		x2 = tmp;
	}

	if (x1 < 0)
		x1 = 0;
	if (x2 >= surf->w)
		x2 = surf->w - 1;

	// small optimization
	uint32_t blackturns2 = SDL_MapRGB(surf->format, R_INCREMENT, G_INCREMENT, B_INCREMENT);

	uint32_t *rowptr = (uint32_t*)((uint8_t *)surf->pixels + y*surf->pitch);
	for (int x = x1; x <= x2; x++) {
		if (rowptr[x] == 0)
			rowptr[x] = blackturns2;
		else {
			uint8_t r, g, b;
			SDL_GetRGB(rowptr[x], surf->format, &r, &g, &b);
			rowptr[x] = SDL_MapRGB(surf->format,
				(uint8_t) min(0xff, (int)r + R_INCREMENT),
				(uint8_t) min(0xff, (int)g + G_INCREMENT),
				(uint8_t) min(0xff, (int)b + B_INCREMENT));
		}
	}
#else
	SDL_FillRect(
		surf, &(SDL_Rect){ min(x1,x2), y, abs(x1-x2), 1 },
		SDL_MapRGB(surf->format, R_INCREMENT, G_INCREMENT, B_INCREMENT));
#endif
}

static int compare_point_y(const void *a, const void *b)
{
	const SDL_Point *p1 = a, *p2 = b;
	return (p1->y > p2->y) - (p1->y < p2->y);
}

void wall_show(const struct Wall *w, const struct Camera *cam)
{
	Vec3 vcor[4];
	get_corners_in_camera_coordinates(w, cam, vcor);

	/*
	Not using floats here caused funny bugs that happened because sorting
	rounded values.
	*/
	struct FPoint fcor[4];
	for (int i = 0; i < 4; i++) {
		if (vcor[i].z >= 0)  // behind camera
			return;
		fcor[i] = camera_point_cam2fpoint(cam, vcor[i]);
	}

	qsort(fcor, 4, sizeof(fcor[0]), compare_point_y);

	struct SDL_Point icor[4];
	for (unsigned i = 0; i < sizeof(fcor)/sizeof(fcor[0]); i++){
		icor[i].x = (int) roundf(fcor[i].x);
		icor[i].y = (int) roundf(fcor[i].y);
	}

	for (int y = icor[0].y; y < icor[1].y; y++)
		draw_hline(cam->surface, line_x(icor[0], icor[1], y), line_x(icor[0], icor[2], y), y);
	for (int y = icor[1].y; y < icor[2].y; y++)
		draw_hline(cam->surface, line_x(icor[1], icor[3], y), line_x(icor[0], icor[2], y), y);
	for (int y = icor[2].y; y < icor[3].y; y++)
		draw_hline(cam->surface, line_x(icor[1], icor[3], y), line_x(icor[2], icor[3], y), y);
}

Vec3 wall_center(const struct Wall *w)
{
	float x = (float)w->startx;
	float y = (Y_MIN + Y_MAX)/2;
	float z = (float)w->startz;

	switch(w->dir) {
	case WALL_DIR_XY:
		x += 0.5f;
		break;
	case WALL_DIR_ZY:
		z += 0.5f;
		break;
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
