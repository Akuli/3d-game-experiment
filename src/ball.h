#ifndef BALL_H
#define BALL_H

#include <stdbool.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "mathstuff.h"

#define BALL_PIXELS_AROUND 200
#define BALL_PIXELS_VERTICALLY 80
#define BALL_RADIUS 0.3f

// This struct is BIG. Always use pointers. Makefile has -Werror=stack-usage=bla
struct Ball {
	Vec3 center;
	SDL_Color image[BALL_PIXELS_VERTICALLY][BALL_PIXELS_AROUND];

	// Applied to the ball before camera transform so that (0,0,0) is ball center.
	Mat3 transform;

	// this is meant only for ball.c
	Vec3 vectorcache[BALL_PIXELS_VERTICALLY + 1][BALL_PIXELS_AROUND];
	bool sidecache[BALL_PIXELS_VERTICALLY + 1][BALL_PIXELS_AROUND];
};

// Does the ball contain the point given in world coordinates?
bool ball_contains(const struct Ball *ball, Vec3 pt);

// Load a ball from an image file. Free it with malloc when done.
struct Ball *ball_load(const char *filename, Vec3 center);

// draw ball to screen if camera is not inside ball
void ball_display(struct Ball *ball, const struct Camera *cam);


#endif  // BALL_H
