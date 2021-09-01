#include "ellipsoid.h"
#include <math.h>
#include <stdbool.h>
#include <SDL2/SDL.h>
#include "log.h"
#include "mathstuff.h"

enum Intersect {
	I_NONE,  // no intersection
	I_TOP,   // bottom of one ellipsoid touches top of another
	I_SIDE,  // bottom of one ellipsoid touches side of another
};

static enum Intersect intersect_in_2d_with_amount_pointer(
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
	We also need a line (disk in 3D) from the same height in the lower ellipsoid.
	It is marked as dashes above. Its ends (x,y) satisfy:
		((x - lcenter.x)/la)^2 + ((y - lcenter.y)/lb)^2 = 1
		y = ucenter.y
	*/
	float halflinelen = la*sqrtf(1 - (botdiff*botdiff)/(lb*lb));

	float overlap = (ua + halflinelen) - fabsf(ucenter.x - lcenter.x);
	if (overlap < 0)
		return I_NONE;
	*olap = overlap;
	return I_SIDE;
}

static enum Intersect intersect_upper_and_lower(
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
	return intersect_in_2d_with_amount_pointer(
		upper->botradius, upper->height, ucenter,
		lower->botradius, lower->height, lcenter,
		olap);
}

bool ellipsoid_intersect(const struct Ellipsoid *el1, const struct Ellipsoid *el2)
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
	return intersect_upper_and_lower(upper, lower, &dummy) != I_NONE;
}

void ellipsoid_move_apart(struct Ellipsoid *el1, struct Ellipsoid *el2)
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
	switch(intersect_upper_and_lower(upper, lower, &olap)) {
		case I_NONE:
			break;
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
}
