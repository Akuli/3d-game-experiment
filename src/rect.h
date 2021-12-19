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
struct RectImage *rect_load_image(const char *path, const SDL_PixelFormat *pixfmt);

#endif   // RECT_H
