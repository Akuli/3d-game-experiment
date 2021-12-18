#include "wall.h"
#include <math.h>
#include "linalg.h"
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

struct Rect3 wall_to_rect(const struct Wall *w)
{
	int dx = (w->dir == WALL_DIR_XY);
	int dz = (w->dir == WALL_DIR_ZY);
	return (struct Rect3){
		.corners = {
			{ w->startx, Y_MIN, w->startz },
			{ w->startx+dx, Y_MIN, w->startz+dz },
			{ w->startx+dx, Y_MAX, w->startz+dz },
			{ w->startx, Y_MAX, w->startz },
		},
	};
}

bool wall_match(const struct Wall *w1, const struct Wall *w2)
{
	return w1->dir == w2->dir && w1->startx == w2->startx && w1->startz == w2->startz;
}


void wall_bumps_ellipsoid(const struct Wall *w, struct Ellipsoid *el)
{
	/*
	If the ellipsoid is very far away from wall, then it surely doesn't bump. We
	use this idea to optimize the common case. But how much is "very far away"?

	Suppose that the ellipsoid and wall intersect at some point p. Let
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
	Vec3 elcenter = mat3_mul_vec3(el->world2uball, el->center);

	for (int xznum = 0; xznum < WALL_CP_COUNT; xznum++) {
		for (int ynum = 0; ynum < WALL_CP_COUNT; ynum++) {
			Vec3 collpoint = mat3_mul_vec3(el->world2uball, w->collpoints[xznum][ynum]);
			Vec3 diff = vec3_sub(elcenter, collpoint);

			float distSQUARED = vec3_lengthSQUARED(diff);
			if (distSQUARED >= 1)   // doesn't bump
				continue;

			float dist = sqrtf(distSQUARED);

			diff.y = 0;   // don't move up/down
			diff = vec3_withlength(diff, 1 - dist);  // move just enough to not touch
			vec3_apply_matrix(&diff, el->uball2world);

			// if we're not bumping on the edge of the wall
			if (xznum != 0 && xznum != WALL_CP_COUNT - 1) {
				// then we should move only in direction opposite to wall
				switch(w->dir) {
					case WALL_DIR_XY: diff.x = 0; break;
					case WALL_DIR_ZY: diff.z = 0; break;
				}
			}

			vec3_add_inplace(&el->center, diff);
			elcenter = mat3_mul_vec3(el->world2uball, el->center);   // cache invalidation
		}
	}
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

extern inline bool wall_linedup(const struct Wall *w1, const struct Wall *w2);
