#ifndef ELLIPSOID_H
#define ELLIPSOID_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "mathstuff.h"


// DON'T MAKE THIS TOO BIG, it uses this^3 amount of memory...
#define ELLIPSOIDPIC_SIDE 150

// picture wrapped around an ellipsoid, may be shared by more than one ellipsoid
// this struct is BIG
struct EllipsoidPic {
	// File name where this came from
	char path[1024];

	/*
	Image pixels stored in whatever pixel format needed for drawing to avoid
	conversions in tight loops (actually made a difference)
	*/
	const SDL_PixelFormat *pixfmt;

	/*
	Which color to show for a given vector? Avoid slow atan2 calls when looking it
	up by providing an array that essentially lets you do cubepixels[x][y][z].
	*/
	uint32_t cubepixels[ELLIPSOIDPIC_SIDE][ELLIPSOIDPIC_SIDE][ELLIPSOIDPIC_SIDE];

	// if true, then only the upper half of the ellipsoid is visible
	bool hidelowerhalf;
};

// epic lmao
void ellipsoidpic_load(
	struct EllipsoidPic *epic, const char *path, const SDL_PixelFormat *fmt);

/*
An Ellipsoid is a stretched ball shifted by the center vector, as in

	((x - center.x) / xzradius)^2 + ((y - center.y) / yradius)^2 + ((z - center.z) / xzradius)^2 = 1.

So, we take the origin-centered unit ball x^2+y^2+z^2=1, stretch it in
x and z directions by xzradius and in y direction by yradius, and move
it to change the center.
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

/*
Information shared in multiple functions, specific to screen x coordinate
*/
struct EllipsoidXCache {
	int screenx;
	float ballcenterscreenx;    // where is ellipsoid->center on screen?
	float xzr;
	const struct Camera *cam;

	// coordinates are in camera coordinates with Ellipsoid.transform_inverse applied
	Vec3 ballcenter;        // ellipsoid->center, transformed as described above
	struct Plane xplane;    // plane of points that are visible at given screen x
	float dSQUARED;         // (distance between xplane and ballcenter)^2
};

// calculate el->transform and el->transform_inverse
void ellipsoid_update_transforms(struct Ellipsoid *el);

/*
Is the ellipsoid visible anywhere on screen? If it is, put range of visible screen
x coordinates to xmin and xmax.
*/
bool ellipsoid_visible_xminmax(
	const struct Ellipsoid *el, const struct Camera *cam, int *xmin, int *xmax);

// Which range of screen y coordinates is showing the ellipsoid? Also fill in xcache.
void ellipsoid_yminmax(
	const struct Ellipsoid *el, const struct Camera *cam,
	int x, struct EllipsoidXCache *xcache,
	int *ymin, int *ymax);

/*
Draw all pixels of ellipsoid corresponding to range of y coordinates. May be
called more than once with same xcache but different ymin and ymax.
*/
void ellipsoid_drawcolumn(
	const struct Ellipsoid *el, const struct EllipsoidXCache *xcache,
	int ymin, int ymax);

/*
Returns how much ellipsoids should be moved apart from each other to make them not
intersect. The moving should happen in xz plane direction (no moving vertically).
Never returns negative. If this returns 0, then the ellipsoids don't intersect
each other.

Currently this does not account for the fact that the lower half of an
ellipsoid can be hidden.
*/
float ellipsoid_bump_amount(const struct Ellipsoid *el1, const struct Ellipsoid *el2);

/*
Move each ellipsoid away from the other one by half of the given amount without
changing y coordinate of location
*/
void ellipsoid_move_apart(struct Ellipsoid *el1, struct Ellipsoid *el2, float mv);


#endif  // ELLIPSOID_H
