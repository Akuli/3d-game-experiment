#ifndef ELLIPSOID_H
#define ELLIPSOID_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "ellipsoidpic.h"
#include "mathstuff.h"

/*
An Ellipsoid is a stretched ball, given by

	((x - center.x) / xzradius)^2 + ((y - center.y) / yradius)^2 + ((z - center.z) / xzradius)^2 = 1,

but shifted by the center vector. So, we take the origin-centered unit ball
x^2+y^2+z^2=1, stretch it in x and z directions by xzradius and in y direction
by yradius, and move it to change the center.
*/
struct Ellipsoid {
	Vec3 center;
	const struct EllipsoidPic *epic;

	// call ellipsoid_update_transforms() after changing these
	float angle;       // radians
	float xzradius;    // positive
	float yradius;     // positive

	/*
	Applying transform to an origin-centered unit ball gives this ellipsoid
	centered at the origin (you still need to add the center vector).
	*/
	Mat3 transform, transform_inverse;
};

// calculate el->transform and el->transform_inverse
void ellipsoid_update_transforms(struct Ellipsoid *el);

bool ellipsoid_visible(const struct Ellipsoid *el, const struct Camera *cam);

/*
Returns how much ellipsoids should be moved apart from each other to make them not
intersect. The moving should happen in xz plane direction (no moving vertically).
Never returns negative. If this returns 0, then the ellipsoids don't intersect
each other.
*/
float ellipsoid_bump_amount(const struct Ellipsoid *el1, const struct Ellipsoid *el2);

/*
Move each ellipsoid away from the other one by half of the given amount without
changing y coordinate of location
*/
void ellipsoid_move_apart(struct Ellipsoid *el1, struct Ellipsoid *el2, float mv);


#endif  // ELLIPSOID_H
