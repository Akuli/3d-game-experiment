#include "intersect.h"
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "mathstuff.h"

// olap not meaningful when returns INTERSECT_NONE
static enum IntersectElEl intersect_2d_ellipses(
	float ua, float ub, Vec2 ucenter,
	float la, float lb, Vec2 lcenter,
	float *olap)
{
	SDL_assert(ua > 0);
	SDL_assert(ub > 0);
	SDL_assert(la > 0);
	SDL_assert(lb > 0);

	float botdiff = ucenter.y - lcenter.y;
	if (botdiff > lb)
		return INTERSECT_EE_NONE;

	/*
	Upper ellipsoid can be treated as a line, because the bottom disk looks like
	that when viewed from the side. We only care about the bottom disk, because
	the rest can't touch the lower ellipsoid.

	                     /              \
	     ,.----..       |                |  <-- Ignore this part
	   /          \     |                |
	 /- - - - - - - \   ==================  <-- These lines matter
	|                |
	|                |
	==================
	*/

	float uleft = ucenter.x - ua;
	float uright = ucenter.x + ua;
	if (uleft <= lcenter.x && lcenter.x <= uright) {
		// They line up vertically
		*olap = lb - botdiff;
		return INTERSECT_EE_BOTTOM1_TIP2;
	}

	/*
	We also need a line (circle in 3D) from the same height in the lower ellipsoid.
	It is marked as dashes above. Its ends (x,y) satisfy:
		((x - lcenter.x)/la)^2 + ((y - lcenter.y)/lb)^2 = 1
		y = ucenter.y
	*/
	float halflinelen = la*sqrtf(1 - (botdiff*botdiff)/(lb*lb));

	*olap = (ua + halflinelen) - fabsf(ucenter.x - lcenter.x);
	return *olap<0 ? INTERSECT_EE_NONE : INTERSECT_EE_BOTTOM1_SIDE2;
}

static enum IntersectElEl intersect_upper_and_lower_el(const struct Ellipsoid *upper, const struct Ellipsoid *lower, Vec3 *mv)
{
	Vec3 dir = vec3_sub(upper->botcenter, lower->botcenter);
	dir.y = 0;
	dir = vec3_withlength(dir, 1);
	if (!isfinite(dir.x) || !isfinite(dir.y) || !isfinite(dir.z)) {
		// Ellipsoids lined up vertically, direction won't really matter
		dir = (Vec3){1, 0, 0};
	}

	// Project everything onto a vertical 2D plane going through the centers of the ellipsoids
	Vec2 ucenter = { vec3_dot(dir, upper->botcenter), upper->botcenter.y };
	Vec2 lcenter = { vec3_dot(dir, lower->botcenter), lower->botcenter.y };
	float olap;
	enum IntersectElEl res = intersect_2d_ellipses(
		upper->botradius, upper->height, ucenter,
		lower->botradius, lower->height, lcenter,
		&olap);

	if (mv) {
		// *mv = how to move upper
		switch(res) {
			case INTERSECT_EE_NONE:
				*mv = (Vec3){ 0, 0, 0 };
				break;
			case INTERSECT_EE_BOTTOM1_TIP2:
				*mv = (Vec3){ 0, olap, 0 };
				break;
			case INTERSECT_EE_BOTTOM1_SIDE2:
			{
				Vec3 low2up = vec3_sub(upper->botcenter, lower->botcenter);
				low2up.y = 0;
				*mv = vec3_withlength(low2up, olap);
				break;
			}
			case INTERSECT_EE_BOTTOM2_TIP1:
			case INTERSECT_EE_BOTTOM2_SIDE1:
				SDL_assert(0);
		}
	}
	return res;
}	

enum IntersectElEl intersect_el_el(const struct Ellipsoid *el1, const struct Ellipsoid *el2, Vec3 *mv)
{
	if (el1->botcenter.y > el2->botcenter.y) {
		return intersect_upper_and_lower_el(el1, el2, mv);
	} else {
		// swapped order, upper ellipsoid must go first
		enum IntersectElEl res = intersect_upper_and_lower_el(el2, el1, mv);
		if(mv)
			*mv = vec3_mul_float(*mv, -1);
		switch(res) {
			case INTERSECT_EE_BOTTOM1_SIDE2: return INTERSECT_EE_BOTTOM2_SIDE1;
			case INTERSECT_EE_BOTTOM1_TIP2: return INTERSECT_EE_BOTTOM2_TIP1;
			default: return res;
		}
	}
}

static bool intersect_circle_and_wall(Vec3 center, float radius, const struct Wall *w, Vec3 *mv)
{
	// Collide against this vertical line between WALL_Y_MIN and WALL_Y_MAX
	float linex, linez;
	if ((w->dir == WALL_DIR_XY && center.x < w->startx) ||
		(w->dir == WALL_DIR_ZY && center.z < w->startz))
	{
		linex = w->startx;
		linez = w->startz;
	} else if (w->dir == WALL_DIR_XY && center.x > w->startx+1) {
		linex = w->startx+1;
		linez = w->startz;
	} else if (w->dir == WALL_DIR_ZY && center.z > w->startz+1) {
		linex = w->startx;
		linez = w->startz+1;
	} else {
		// Bottom circle lines up with wall, move perpendicularly to wall
		switch(w->dir) {
			case WALL_DIR_XY:
			{
				float diff = center.z - w->startz;
				if (fabsf(diff) < radius) {
					if (mv)
						*mv = (Vec3){ 0, 0, copysignf(radius, diff) - diff };
					return true;
				}
				break;
			}
			case WALL_DIR_ZY:
			{
				float diff = center.x - w->startx;
				if (fabsf(diff) < radius) {
					if (mv)
						*mv = (Vec3){ copysignf(radius, diff) - diff, 0, 0 };
					return true;
				}
				break;
			}
		}
		return false;
	}

	Vec3 edgepoint = { linex, center.y, linez };
	Vec3 edge2center = vec3_sub(center, edgepoint);
	if (vec3_lengthSQUARED(edge2center) < radius*radius) {
		if (mv)
			*mv = vec3_sub(vec3_withlength(edge2center, radius), edge2center);
		return true;
	}
	return false;
}

enum IntersectElWall intersect_el_wall(const struct Ellipsoid *el, const struct Wall *w, Vec3 *mv)
{
	/*
	If the ellipsoid is very far away from wall, then it surely doesn't bump. We
	use this idea to optimize the common case. But how much is "very far away"?

	Suppose that the ellipsoid and wall intersect at some point p. Let
	diam(w) denote the distance between opposite corners of a wall. Then

			|center(w) - bottom_center(el)|
		=	|center(w) - p  +  p - bottom_center(el)|    (because -p+p = zero vector)
		<=	|center(w) - p| + |p - bottom_center(el)|    (by triangle inequality)
		<=	diam(w)/2       + |p - bottom_center(el)|    (because p is in wall)
		<=	diam(w)/2       + max(botradius, height)     (because p is in ellipsoid)

	If this is not the case, then we can't have any intersections.
	*/
	float diam = hypotf(WALL_Y_MAX - WALL_Y_MIN, 1);
	float lenbound = diam/2 + max(el->botradius, el->height);
	if (vec3_lengthSQUARED(vec3_sub(el->botcenter, wall_center(w))) > lenbound*lenbound)
		return INTERSECT_EW_NONE;

	if (el->botcenter.y + el->height < WALL_Y_MIN || el->botcenter.y > WALL_Y_MAX)
		return INTERSECT_EW_NONE;

	if (el->botcenter.y > WALL_Y_MIN) {
		// Use bottom circle
		// FIXME: how to decide INTERSECT_EW_ELSIDE vs INTERSECT_EW_ELBOTTOM (#84)
		if (intersect_circle_and_wall(el->botcenter, el->botradius, w, mv))
			return INTERSECT_EW_ELSIDE;
		return INTERSECT_EW_NONE;
	} else {
		/*
		Use a slice of the ellipsoid as the circle:
		     ,.----..
		   /          \
		 /--------------\------- y=WALL_Y_MIN
		|                |
		|                |
		==================

		To find equation for radius r, consider the intersection points given by:

			(x/a)^2 + (y/b)^2 = 1
			y = ydiff

		where x and y are shifted so that (0,0) means center of bottom circle.
		In these coordinates, the intersection points are +r and -r.
		*/
		float ydiff = WALL_Y_MIN - el->botcenter.y;
		float a = el->botradius;
		float b = el->height;
		float r = a * sqrtf(1 - (ydiff*ydiff)/(b*b));
		Vec3 center = { el->botcenter.x, WALL_Y_MIN, el->botcenter.z };
		if (intersect_circle_and_wall(center, r, w, mv))
			return INTERSECT_EW_ELSIDE;
		return INTERSECT_EW_NONE;
	}
}
