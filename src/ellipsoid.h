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
	up by providing an array that essentially lets you do cubepixels[highlighted][x][y][z].
	Here highlighted is usually 0, but can be 1 for different color.
	*/
	uint32_t cubepixels[2][ELLIPSOIDPIC_SIDE][ELLIPSOIDPIC_SIDE][ELLIPSOIDPIC_SIDE];
};

// epic lmao
// free(epic) to unload
void ellipsoidpic_load(struct EllipsoidPic *epic, const char *path, const SDL_PixelFormat *fmt);

// resulting array of pointers is freed with atexit()
struct EllipsoidPic *const *ellipsoidpic_loadmany(int *n, const char *globpat, const SDL_PixelFormat *fmt);

/*
An Ellipsoid is a stretched ball shifted by the center vector, as in

	((x - center.x) / botradius)^2 + ((y - center.y) / height)^2 + ((z - center.z) / botradius)^2 = 1

	y >= center.y

So, we take the origin-centered unit ball x^2+y^2+z^2=1, delete the lower
half, stretch it in x and z directions by bottom radius and in y direction
by height, and move it to change the center.
*/
struct Ellipsoid {
	Vec3 botcenter;  // center of bottom circle
	const struct EllipsoidPic *epic;
	bool highlighted;

	// call ellipsoid_update_transforms() after changing these
	float angle;
	float botradius;
	float height;

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
	float botcenterscreenx;    // where is ellipsoid->center on screen?
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

bool ellipsoid_intersect(const struct Ellipsoid *el1, const struct Ellipsoid *el2);

// Move ellipsoids so that they no longer intersect
void ellipsoid_move_apart(struct Ellipsoid *el1, struct Ellipsoid *el2);


#endif  // ELLIPSOID_H
