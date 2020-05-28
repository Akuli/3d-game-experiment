#include "wall.h"
#include <assert.h>
#include "mathstuff.h"
#include "player.h"
#include <SDL2/SDL2_gfxPrimitives.h>


static void get_points(const struct Wall *w, Vec3 *points)
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
	}

	points[0] = (Vec3){ (float)w->startx, 0, (float)w->startz };
	points[1] = (Vec3){ (float)w->startx, 1, (float)w->startz };
	points[2] = (Vec3){ (float)endx,      1, (float)endz      };
	points[3] = (Vec3){ (float)endx,      0, (float)endz      };
}

static float linear_map(float srcmin, float srcmax, float dstmin, float dstmax, float val)
{
	float ratio = (val - srcmin)/(srcmax - srcmin);
	return dstmin + ratio*(dstmax - dstmin);
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

/*static void draw_triangle(SDL_Surface *surf, SDL_Point p1, SDL_Point p2, SDL_Point p3)
{
	if (abs(p2.x - p3.x) > abs(p2.y - p3.y)) {
		if (p2.x > p3.x)
			swap(&p2, &p3);
		for (int x = p2.x; x <= p3.x; x++) {
			int y = linear_map(p2.x, p2.y, p3.x, p3.y, x);
			SDL_FillRect(surf, (SDL_Rect){ x, y, 1, 1 });
		}
	} else {
		if (p2.y > p3.y)
			swap(&p2, &p3);
		for (int x = p2.y; x <= p3.y; y++) {
			int y = linear_map(p2.x, p2.y, p3.x, p3.y, x);
			SDL_Draw
			SDL_FillRect(surf, (SDL_Rect){ x, y, 1, 1 });
		}
	}
}
*/

void wall_show(const struct Wall *w, const struct Camera *cam)
{
	for (int xznum = 0; xznum < WALL_CP_COUNT; xznum++) {
		for (int ynum = 0; ynum < WALL_CP_COUNT; ynum++) {
			Vec3 worldvec = camera_point_world2cam(cam, w->collpoint_cache[xznum][ynum]);
			if (worldvec.z < 0) {
				SDL_Point p = camera_point_to_sdl(cam, worldvec);
				SDL_FillRect(
					cam->surface,
					&(SDL_Rect){ p.x, p.y, 1, 1 },
					SDL_MapRGBA(cam->surface->format, 0xff, 0xff, 0xff, 0xff));
			}
		}
	}
}
