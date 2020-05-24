#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <SDL2/SDL.h>
#include "vecmat.h"

#define DISPLAY_WIDTH 800
#define DISPLAY_HEIGHT 600
#define DISPLAY_SCALING_FACTOR 20000.f

/*
When drawing a pixel of the screen, the 3D points that could be drawn to that pixel
form a line. We can write the equation of that line in the following form:

This won't do well with e.g. lines going in the direction of the x or y axis,
because their equations can't be written like this.

The above equations actually specify an intersection of two planes. A horizontal
such plane represents all the points that are shown on a given horizontal row of
pixels. Note that the equation containing x specifies a *vertical* plane, since it
doesn't contain any restrictions for values of y.

Both of the added numbers are 0, because the lines pass through the camera, and
the camera is at (0,0,0). This means that we actually have:

	x = (some number)*z      (vertical plane)
	y = (some number)*z      (horizontal plane)

and we only need one number to represent these. The number here is

	some number = x/z,

so I call it the "x to z ratio", or xzr for short, and similarly yzr.
*/

// Conversion between screen coordinates and plane x/z and y/z ratios
float display_screenx_to_xzr(float screenx);
float display_screeny_to_yzr(float screeny);
float display_xzr_to_screenx(float xzr);
float display_yzr_to_screeny(float yzr);

// Where on the screen should a 3D point be shown?
SDL_Point display_point_to_sdl(struct Vec3 pt);

/*
The points must be laid out something like this:

	point 1    point 3

	point 2    point 4

Not like this:

	point 1    point 4

	point 2    point 3
*/
struct Display4Gon {
	struct Vec3 point1, point2, point3, point4;
};

enum DisplayKind {
	DISPLAY_BORDER,   // fast
	DISPLAY_FILLED,   // slow
	DISPLAY_RECT,     // fast
};

void display_4gon(SDL_Renderer *rnd, struct Display4Gon gon, SDL_Color col, enum DisplayKind dk);


#endif    // DISPLAY_H
