#ifndef BALL_H
#define BALL_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "mathstuff.h"

#define BALL_PIXELS_AROUND 200
#define BALL_PIXELS_VERTICALLY 80

// This struct is BIG. Always use pointers. Makefile has -Werror=stack-usage=bla
struct Ball {
	Vec3 center;
	float angle;    // 0 = looking toward negative direction of z axis
	SDL_Color image[BALL_PIXELS_VERTICALLY][BALL_PIXELS_AROUND];

	// this is meant only for ball.c
	Vec3 vectorcache[BALL_PIXELS_VERTICALLY + 1][BALL_PIXELS_AROUND];
	bool sidecache[BALL_PIXELS_VERTICALLY + 1][BALL_PIXELS_AROUND];
};

// Does the ball contain the point?
bool ball_contains(const struct Ball *sph, Vec3 pt);

// Load a ball from an image file. Free it with malloc when done.
struct Ball *ball_load(const char *filename, Vec3 center);

// draw ball to screen if camera is not inside ball
void ball_display(struct Ball *sph, const struct Camera *cam);


#endif  // BALL_H
