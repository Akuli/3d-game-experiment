#include "intersect.h"
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "mathstuff.h"

enum Intersect {
	I_NONE,  // no intersection
	I_TOP,   // bottom of one ellipsoid touches top of another
	I_SIDE,  // bottom of one ellipsoid touches side of another
};

// olap not meaningful when returns I_NONE
static enum Intersect intersect_2d_ellipses(
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
		return I_NONE;

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
		return I_TOP;
	}

	/*
	We also need a line (circle in 3D) from the same height in the lower ellipsoid.
	It is marked as dashes above. Its ends (x,y) satisfy:
		((x - lcenter.x)/la)^2 + ((y - lcenter.y)/lb)^2 = 1
		y = ucenter.y
	*/
	float halflinelen = la*sqrtf(1 - (botdiff*botdiff)/(lb*lb));

	*olap = (ua + halflinelen) - fabsf(ucenter.x - lcenter.x);
	return *olap<0 ? I_NONE : I_SIDE;
}

static enum Intersect intersect_upper_and_lower_ellipsoids(
	const struct Ellipsoid *upper, const struct Ellipsoid *lower, float *olap)
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
	return intersect_2d_ellipses(
		upper->botradius, upper->height, ucenter,
		lower->botradius, lower->height, lcenter,
		olap);
}

bool intersect_check_el_el(const struct Ellipsoid *el1, const struct Ellipsoid *el2)
{
	const struct Ellipsoid *upper, *lower;
	if (el1->botcenter.y > el2->botcenter.y) {
		upper = el1;
		lower = el2;
	} else {
		upper = el2;
		lower = el1;
	}

	float dummy;
	return intersect_upper_and_lower_ellipsoids(upper, lower, &dummy) != I_NONE;
}

bool intersect_move_el_el(struct Ellipsoid *el1, struct Ellipsoid *el2)
{
	struct Ellipsoid *upper, *lower;
	if (el1->botcenter.y > el2->botcenter.y) {
		upper = el1;
		lower = el2;
	} else {
		upper = el2;
		lower = el1;
	}

	float olap;
	switch(intersect_upper_and_lower_ellipsoids(upper, lower, &olap)) {
		case I_NONE:
			return false;
		case I_TOP:
			upper->botcenter.y += olap;
			break;
		case I_SIDE:
		{
			Vec3 low2up = vec3_sub(upper->botcenter, lower->botcenter);
			low2up.y = 0;
			vec3_add_inplace(&upper->botcenter, vec3_withlength(low2up, olap));
			break;
		}
	}
	return true;
}

// Change *val if needed so that it is not between center-r and center+r
static bool ensure_not_near(float *val, float center, float r)
{
	if (fabsf(center - *val) < r) {
		if (*val > center)
			*val = center + r;
		else
			*val = center - r;
		return true;
	}
	return false;
}

static bool ensure_circle_not_hitting_wall(Vec3 *center, float radius, const struct Wall *w)
{
	// Collide against this vertical line between WALL_Y_MIN and WALL_Y_MAX
	float linex, linez;
	if ((w->dir == WALL_DIR_XY && center->x < w->startx) ||
		(w->dir == WALL_DIR_ZY && center->z < w->startz))
	{
		linex = w->startx;
		linez = w->startz;
	} else if (w->dir == WALL_DIR_XY && center->x > w->startx+1) {
		linex = w->startx+1;
		linez = w->startz;
	} else if (w->dir == WALL_DIR_ZY && center->z > w->startz+1) {
		linex = w->startx;
		linez = w->startz+1;
	} else {
		// Bottom circle lines up with wall, move perpendicularly to wall
		switch(w->dir) {
			case WALL_DIR_XY: return ensure_not_near(&center->z, w->startz, radius); break;
			case WALL_DIR_ZY: return ensure_not_near(&center->x, w->startx, radius); break;
		}
		return false;  // never runs, but makes compiler happy
	}

	Vec3 edgepoint = { linex, center->y, linez };
	Vec3 edge2center = vec3_sub(*center, edgepoint);
	if (vec3_lengthSQUARED(edge2center) < radius*radius) {
		edge2center = vec3_withlength(edge2center, radius);
		*center = vec3_add(edgepoint, edge2center);
		return true;
	}
	return false;
}

bool intersect_move_el_wall(struct Ellipsoid *el, const struct Wall *w)
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
		return false;

	if (el->botcenter.y + el->height < WALL_Y_MIN || el->botcenter.y > WALL_Y_MAX)
		return false;

	if (el->botcenter.y > WALL_Y_MIN) {
		// Use bottom circle
		return ensure_circle_not_hitting_wall(&el->botcenter, el->botradius, w);
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
		Vec3 oldcenter = center;
		if (ensure_circle_not_hitting_wall(&center, r, w)) {
			vec3_add_inplace(&el->botcenter, vec3_sub(center, oldcenter));
			return true;
		}
		return false;
	}
}
