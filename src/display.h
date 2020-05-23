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
form a line. This struct is intended to represent that in coordinates where the z
axis is in the direction of the player. This won't do well with e.g. lines going in
the direction of the x or y axis, because their equations can be written as

	x = xcoeff*z + xconst
	y = ycoeff*z + yconst,

which is how this struct represents lines.
*/
struct DisplayLine {
	float xcoeff, xconst, ycoeff, yconst;
};

// Given line and z coordinate, calculate corresponding x and y coords
struct Vec3 displayline_z2point(struct DisplayLine ln, float z);

/*
Construct a line representing the 3D points that correspond to a pixel on the screen.
*/
struct DisplayLine displayline_frompixel(int screenx, int screeny);

/*
Move the line by a vector
*/
void displayline_move(struct DisplayLine *ln, struct Vec3 mv);

/*
Example: 0xabcdef means 0xab for red, 0xcd for green, 0xef for blue.

This uses only 24 of the available 32 bits, but that's fine imo. An advantage of
this is that -1 can be used to represent a "nothing" color.
*/
typedef int32_t displaycolor;

SDL_Color displaycolor2sdl(int32_t displaycolor);


#endif    // DISPLAY_H
