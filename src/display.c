#include "display.h"
#include <assert.h>
#include <SDL2/SDL.h>

struct Vec3 displayline_z2point(struct DisplayLine ln, float z)
{
	return (struct Vec3){
		.x = ln.xcoeff*z + ln.xconst,
		.y = ln.ycoeff*z + ln.yconst,
		.z = z,
	};
}

/*
The mapping from a 3D point (x,y,z) to a screen point (screenx,screeny) is:

	screenx = DISPLAY_WIDTH/2 + DISPLAY_SCALING_FACTOR*x/z
	screeny = DISPLAY_HEIGHT/2 - DISPLAY_SCALING_FACTOR*y/z

Here the positive z direction is where the player is looking.

I call these the screen equations. Keep that in mind while reading comments.
*/
struct DisplayLine displayline_frompixel(int screenx, int screeny)
{
	/*
	Solving screen equations for x and y gives

		x = ( screenx - DISPLAY_WIDTH /2)/DISPLAY_SCALING_FACTOR * z
		y = (-screeny + DISPLAY_HEIGHT/2)/DISPLAY_SCALING_FACTOR * z
	*/
	return (struct DisplayLine){
		.xcoeff = (float)(screenx - DISPLAY_WIDTH/2) / DISPLAY_SCALING_FACTOR,
		.ycoeff = (float)(-screeny + DISPLAY_HEIGHT/2) / DISPLAY_SCALING_FACTOR,
		.xconst = 0,
		.yconst = 0,
	};
}

void displayline_move(struct DisplayLine *ln, struct Vec3 mv)
{
	/*
	Generally moving changes the equation of an object so that x gets replaced with
	x - mv.x, y gets replaced with y - mv.y, and you guess what happens to z.
	Doing that to the DisplayLine equations

		x = xcoeff*z + xconst
		y = ycoeff*z + yconst

	gives

		x - mv.x = xcoeff*(z - mv.z) + xconst
		y - mv.y = ycoeff*(z - mv.z) + yconst

	which can be rewritten like this:

		x = xcoeff*z + xconst + mv.x - xcoeff*mv.z
		y = ycoeff*z + yconst + mv.y - ycoeff*mv.z
	*/
	ln->xconst += mv.x - ln->xcoeff*mv.z;
	ln->yconst += mv.y - ln->ycoeff*mv.z;
}

SDL_Color displaycolor2sdl(int32_t displaycolor)
{
	assert(0 <= displaycolor && displaycolor <= 0xffffff);
	return (SDL_Color){
		.r = (uint8_t)((displaycolor & 0xff0000) >> 16),  // extract AB of 0xABCDEF
		.g = (uint8_t)((displaycolor & 0x00ff00) >> 8),   // extract CD of 0xABCDEF
		.b = (uint8_t)((displaycolor & 0x0000ff) >> 0),   // extract EF of 0xABCDEF
		.a = 0xff,  // no transparency
	};
}
