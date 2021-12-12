// rectangle in 3D, e.g. wall
#ifndef RECT_H
#define RECT_H

#include <stdint.h>
#include <SDL2/SDL.h>
#include "mathstuff.h"

struct RectImage {
	int width;
	int height;
	uint32_t data[];
};

struct Rect {
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
	const struct RectImage *img;
};

// use free()
struct RectImage *rect_load_image(const char *path, SDL_PixelFormat *pixfmt);

void rect_drawborder(const struct Rect3 *r, const struct Camera *cam);

struct RectCache {
	const struct Rect *rect;
	const struct Camera *cam;
	Vec2 screencorners[4];
};

// Returns whether the rect is visible. If true, fills the cache.
bool rect_visible_fillcache(const struct Rect *r, const struct Camera *cam, struct RectCache *cache);

// Before drawing, xmin and xmax can be replaced with a subinterval.
// If not visible at all for given y, xminmax will return false.
bool rect_xminmax(const struct RectCache *cache, int y, int *xmin, int *xmax);
void rect_drawrow(const struct RectCache *cache, int y, int xmin, int xmax);

#endif   // RECT_H
