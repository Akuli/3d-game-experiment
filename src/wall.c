#include "wall.h"
#include <assert.h>
#include <stdint.h>
#include "camera.h"
#include "mathstuff.h"
#include "player.h"
#include <SDL2/SDL2_gfxPrimitives.h>


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
			ptr->y = linear_map(0, WALL_CP_COUNT-1, PLAYER_HEIGHT_FLAT, WALL_HEIGHT, (float)ynum);
			ptr->z = (float)w->startz;

			switch(w->dir) {
			case WALL_DIR_X:
				ptr->x += linear_map(0, WALL_CP_COUNT-1, 0, 1, (float)xznum);
				break;
			case WALL_DIR_Z:
				ptr->z += linear_map(0, WALL_CP_COUNT-1, 0, 1, (float)xznum);
				break;
			}
		}
	}
}

void wall_bumps_ball(const struct Wall *w, struct Ball *ball)
{
	// Switch to coordinates where the ball is round and we can use BALL_RADIUS
	Mat3 invtrans = mat3_inverse(ball->transform);
	Vec3 center = mat3_mul_vec3(invtrans, ball->center);

	for (int xznum = 0; xznum < WALL_CP_COUNT; xznum++) {
		for (int ynum = 0; ynum < WALL_CP_COUNT; ynum++) {
			Vec3 collpoint = mat3_mul_vec3(invtrans, w->collpoint_cache[xznum][ynum]);
			Vec3 diff = vec3_sub(center, collpoint);

			float distSQUARED = vec3_lengthSQUARED(diff);
			if (distSQUARED >= BALL_RADIUS*BALL_RADIUS)
				continue;

			float dist = sqrtf(distSQUARED);

			diff.y = 0;   // don't move up/down
			diff = vec3_withlength(diff, BALL_RADIUS - dist);  // move just enough to not touch
			vec3_apply_matrix(&diff, ball->transform);

			// if we're not bumping on the edge of the wall
			if (xznum != 0 && xznum != WALL_CP_COUNT - 1) {
				// then we should move only in direction opposite to wall
				switch(w->dir) {
					case WALL_DIR_X: diff.x = 0; break;
					case WALL_DIR_Z: diff.z = 0; break;
				}
			}

			vec3_add_inplace(&ball->center, diff);
			center = mat3_mul_vec3(invtrans, ball->center);   // cache invalidation
		}
	}
}

static int compare_point_y(const void *a, const void *b)
{
	const SDL_Point *p1 = a, *p2 = b;
	return (p1->y > p2->y) - (p1->y < p2->y);
}

static bool get_corners(const struct Wall *w, const struct Camera *cam, struct FPoint *res)
{
	int endx, endz;
	switch(w->dir) {
	case WALL_DIR_X:
		endx = w->startx + 1;
		endz = w->startz;
		break;
	case WALL_DIR_Z:
		endx = w->startx;
		endz = w->startz + 1;
		break;
	default:
		// never actually happens, make compiler happy
		assert(0);
		endx = endz = 0;
	}

	Vec3 tmp[] = {
#define f(x,y,z) camera_point_world2cam(cam, (Vec3){ (float)x, (float)y, (float)z })
		f(w->startx, 0, w->startz),
		f(w->startx, 1, w->startz),
		f(endx,      0, endz),
		f(endx,      1, endz),
#undef f
	};

	for (unsigned i = 0; i < sizeof(tmp)/sizeof(tmp[0]); i++) {
		if (tmp[i].z >= 0)  // behind camera
			return false;
		res[i] = camera_point_cam2fpoint(cam, tmp[i]);
	}

	return true;
}

static int line_x(SDL_Point p1, SDL_Point p2, int y)
{
	if (p1.y == p2.y)
		return p1.x;   // avoid division by zero
	return linear_map_int(p1.y, p2.y, p1.x, p2.x, y);
}

#define min(a,b) ((a)<(b) ? (a) : (b))

static void draw_hline(SDL_Surface *surf, int x1, int x2, int y, uint32_t col)
{
	SDL_FillRect(surf, &(SDL_Rect){ min(x1, x2), y, abs(x1-x2), 1 }, col);
}

void wall_show(const struct Wall *w, const struct Camera *cam)
{
	uint32_t col = SDL_MapRGB(cam->surface->format, 0, 0xff, 0);

	struct FPoint fcor[4];
	if (!get_corners(w, cam, fcor))
		return;

	qsort(fcor, 4, sizeof(fcor[0]), compare_point_y);

	struct SDL_Point icor[sizeof(fcor)/sizeof(fcor[0])];
	for (unsigned i = 0; i < sizeof(fcor)/sizeof(fcor[0]); i++){
		icor[i].x = (int) roundf(fcor[i].x);
		icor[i].y = (int) roundf(fcor[i].y);
	}

	for (int y = icor[0].y; y < icor[1].y; y++)
		draw_hline(cam->surface, line_x(icor[0], icor[1], y), line_x(icor[0], icor[2], y), y, col);
	for (int y = icor[1].y; y < icor[2].y; y++)
		draw_hline(cam->surface, line_x(icor[1], icor[3], y), line_x(icor[0], icor[2], y), y, col);
	for (int y = icor[2].y; y < icor[3].y; y++)
		draw_hline(cam->surface, line_x(icor[1], icor[3], y), line_x(icor[2], icor[3], y), y, col);
}
