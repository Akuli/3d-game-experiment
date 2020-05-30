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

	/*
	Transform is applied to the ball before camera transform so that (0,0,0)
	is ball center. Remember to keep transform_inverse up to date.
	*/
	Mat3 transform, transform_inverse;

	/*
	These are meant only for ball.c. They are here because it's handy to allocate
	them along with rest of the ball.
	*/
	Vec3 vectorcache[BALL_PIXELS_VERTICALLY + 1][BALL_PIXELS_AROUND];
	bool sidecache[BALL_PIXELS_VERTICALLY + 1][BALL_PIXELS_AROUND];
};

// Load a ball from an image file. Free it with malloc when done.
struct Ball *ball_load(const char *filename, Vec3 center);

// draw ball to screen if camera is not inside ball
void ball_display(struct Ball *ball, const struct Camera *cam);

/*
Find intersection point of ball and line, returning false if no intersection.

Typically the line enters the ball somewhere and exits the ball at some other
point. This gives the "average point" (i.e. point in the middle) of the entering
and exiting points in that case.
*/
bool ball_intersect_line(const struct Ball *ball, struct Line ln, Vec3 *res);


#endif  // BALL_H
