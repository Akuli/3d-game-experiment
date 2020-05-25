#include "display.h"
#include <assert.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>   // sudo apt install libsdl2-gfx-dev

#define SCALING_FACTOR 300.f

/*
The mapping from a 3D point (x,y,z) to a screen point (screenx,screeny) is:

	screenx = DISPLAY_WIDTH/2 + SCALING_FACTOR*x/-z
	screeny = DISPLAY_HEIGHT/2 - SCALING_FACTOR*y/-z

Here the NEGATIVE z direction is where the player is looking.
*/


float display_xzr_to_screenx(float xzr) { return DISPLAY_WIDTH/2 - SCALING_FACTOR*xzr; }
float display_yzr_to_screeny(float yzr) { return DISPLAY_HEIGHT/2 + SCALING_FACTOR*yzr; }

float display_screenx_to_xzr(float screenx) { return (-screenx + DISPLAY_WIDTH/2)/SCALING_FACTOR; }
float display_screeny_to_yzr(float screeny) { return (screeny - DISPLAY_HEIGHT/2)/SCALING_FACTOR; }

SDL_Point display_point_to_sdl(struct Vec3 pt)
{
	assert(pt.z < 0);
	return (SDL_Point){
		.x = (int)display_xzr_to_screenx(pt.x/pt.z),
		.y = (int)display_yzr_to_screeny(pt.y/pt.z),
	};
}

static uint32_t convert_color_for_gfx(SDL_Color col)
{
	// docs say that it's 0xRRGGBBAA but actual behaviour is 0xAABBGGRR... wtf
	return ((uint32_t)col.a << 24) | ((uint32_t)col.b << 16) | ((uint32_t)col.g << 8) | ((uint32_t)col.r << 0);
}

#define min(a,b) ((a)<(b) ? (a) : (b))
#define max(a,b) ((a)>(b) ? (a) : (b))
#define min4(a,b,c,d) min(min(a,b),min(c,d))
#define max4(a,b,c,d) max(max(a,b),max(c,d))

static void point_to_screen_floats(struct Vec3 pt, float *x, float *y)
{
	*x = display_xzr_to_screenx(pt.x / pt.z);
	*y = display_yzr_to_screeny(pt.y / pt.z);
}

void display_4gon(const struct Camera *cam, struct Display4Gon gon, SDL_Color col, enum DisplayKind dk)
{
	// must be in front of the camera, i.e. negative z
	if (gon.point1.z >= 0 || gon.point2.z >= 0 || gon.point3.z >= 0 || gon.point4.z >= 0)
		return;

	float p1x, p1y, p2x, p2y, p3x, p3y, p4x, p4y;
	point_to_screen_floats(gon.point1, &p1x, &p1y);
	point_to_screen_floats(gon.point2, &p2x, &p2y);
	point_to_screen_floats(gon.point3, &p3x, &p3y);
	point_to_screen_floats(gon.point4, &p4x, &p4y);

	float xmin = min4(p1x, p2x, p3x, p4x);
	float xmax = max4(p1x, p2x, p3x, p4x);
	float ymin = min4(p1y, p2y, p3y, p4y);
	float ymax = max4(p1y, p2y, p3y, p4y);

	// note the order 1,2,4,3, this is because sdl and my Display4Gon differ
	int16_t xarr[] = { (int16_t)p1x, (int16_t)p2x, (int16_t)p4x, (int16_t)p3x };
	int16_t yarr[] = { (int16_t)p1y, (int16_t)p2y, (int16_t)p4y, (int16_t)p3y };

	switch(dk) {
	case DISPLAY_BORDER:
		polygonColor(cam->renderer, xarr, yarr, 4, convert_color_for_gfx(col));
		break;

	case DISPLAY_FILLED:
		/*
		This function is VERY SLOW. I tried implementing it myself and my
		implementation was even slower :D
		*/
		filledPolygonColor(cam->renderer, xarr, yarr, 4, convert_color_for_gfx(col));
		break;

	case DISPLAY_RECT:
		SDL_SetRenderDrawColor(cam->renderer, col.r, col.g, col.b, col.a);
		SDL_RenderFillRect(cam->renderer, &(SDL_Rect){
			(int)floorf(xmin),
			(int)floorf(ymin),
			(int)ceilf(xmax-xmin),
			(int)ceilf(ymax-ymin),
		});
		break;
	}
}
