// picture wrapped around an ellipsoid, may be shared by more than one ellipsoid

#ifndef ELLIPSOIDPIC_H
#define ELLIPSOIDPIC_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>

// DON'T MAKE THIS TOO BIG, it uses this^3 amount of memory...
#define ELLIPSOIDPIC_SIDE 150

// this struct is BIG
struct EllipsoidPic {
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

void ellipsoidpic_load(
	struct EllipsoidPic *epic, const char *filename, const SDL_PixelFormat *fmt);


#endif    // ELLIPSOIDPIC_H
