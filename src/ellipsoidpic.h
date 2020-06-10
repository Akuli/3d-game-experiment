// picture wrapped around an ellipsoid, may be shared by more than one ellipsoid

#ifndef ELLIPSOIDPIC_H
#define ELLIPSOIDPIC_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>

#define ELLIPSOIDPIC_HEIGHT 80
#define ELLIPSOIDPIC_AROUND 200

// this struct is BIG
struct EllipsoidPic {
	/*
	Image pixels stored in whatever pixel format needed for drawing to avoid
	conversions in tight loops (actually made a difference)
	*/
	const SDL_PixelFormat *pixfmt;
	uint32_t pixels[ELLIPSOIDPIC_HEIGHT][ELLIPSOIDPIC_AROUND];

	// if true, then only the upper half of the ellipsoid is visible
	bool hidelowerhalf;
};

void ellipsoidpic_load(
	struct EllipsoidPic *epic, const char *filename, const SDL_PixelFormat *fmt);


#endif    // ELLIPSOIDPIC_H
