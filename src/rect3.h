// rectangle in 3D, e.g. wall
#ifndef RECT_H
#define RECT_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "linalg.h"
#include "camera.h"

struct Rect3 {
	/*
	Corners must be in the same plane and in a cycling order, e.g.

		corners[0] --- corners[1]
		    |              |
		    |              |
		corners[3] --- corners[2]

	or

		corners[0] --- corners[3]
		    |              |
		    |              |
		corners[1] --- corners[2]
	*/
	Vec3 corners[4];
};

void rect3_drawborder(const struct Rect3 *r, const struct Camera *cam);

struct Rect3Cache {
	const struct Rect3 *rect;
	const struct Camera *cam;
	Vec2 screencorners[4];
	SDL_Rect bbox;  // will contain everything that gets drawn
};

// Returns whether the rect is visible. If true, fills the cache.
bool rect3_visible_fillcache(const struct Rect3 *r, const struct Camera *cam, struct Rect3Cache *cache);

// Before drawing, xmin and xmax can be replaced with a subinterval.
// If not visible at all for given y, xminmax will return false.
bool rect3_xminmax(const struct Rect3Cache *cache, int y, int *xmin, int *xmax);
void rect3_drawrow(const struct Rect3Cache *cache, int y, int xmin, int xmax, bool highlight);

// In camera coordinates, returns z of intersection with line t*(xzr,yzr,1)
float rect3_get_camcoords_z(const struct Rect3 *r, const struct Camera *cam, float xzr, float yzr);

#endif   // RECT_H
