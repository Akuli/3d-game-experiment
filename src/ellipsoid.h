#ifndef ELLIPSOID_H
#define ELLIPSOID_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "mathstuff.h"

#define ELLIPSOID_PIXELS_AROUND 200
#define ELLIPSOID_PIXELS_VERTICALLY 80

/*
An Ellipsoid is a stretched ball, given by

	((x - center.x) / xzradius)^2 + ((y - center.y) / yradius)^2 + ((z - center.z) / xzradius)^2 = 1,

but shifted by the center vector. So, we take the origin-centered unit ball
x^2+y^2+z^2=1, stretch it in x and z directions by xzradius and in y direction
by yradius, and move it to change the center.

This struct is BIG. Always use pointers. Makefile has -Werror=stack-usage=bla
*/
struct Ellipsoid {
	Vec3 center;
	SDL_Color image[ELLIPSOID_PIXELS_VERTICALLY][ELLIPSOID_PIXELS_AROUND];

	// call ellipsoid_update_transforms() after changing these
	float angle;       // radians
	float xzradius;    // positive
	float yradius;     // positive

	/*
	Applying transform to an origin-centered unit ball gives this ellipsoid
	centered at the origin (you still need to add the center vector).
	*/
	Mat3 transform, transform_inverse;

	/*
	These are meant only for ellipsoid.c. They are here because it's handy to allocate
	them along with rest of the ellipsoid.
	*/
	Vec3 vectorcache[ELLIPSOID_PIXELS_VERTICALLY + 1][ELLIPSOID_PIXELS_AROUND];
	bool sidecache[ELLIPSOID_PIXELS_VERTICALLY + 1][ELLIPSOID_PIXELS_AROUND];
};

// calculate el->transform and el->transform_inverse
void ellipsoid_update_transforms(struct Ellipsoid *el);

// Load a ellipsoid from an image file. Free it with malloc when done.
struct Ellipsoid *ellipsoid_load(const char *filename, Vec3 center);

// draw ellipsoid to screen if camera is not inside ellipsoid
void ellipsoid_show(struct Ellipsoid *el, const struct Camera *cam);

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
