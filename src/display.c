#include "display.h"
#include <assert.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>   // sudo apt install libsdl2-gfx-dev

#define SCALING_FACTOR 300.f

/*
The mapping from a 3D point (x,y,z) to a screen point (screenx,screeny) is:

	screenx = screenwidth/2 + SCALING_FACTOR*x/-z
	screeny = screenheight/2 - SCALING_FACTOR*y/-z

Here the NEGATIVE z direction is where the player is looking (i.e. we use camera
coordinates).
*/


float display_xzr_to_screenx(SDL_Surface *surf, float xzr) { return (float)surf->w/2 - SCALING_FACTOR*xzr; }
float display_yzr_to_screeny(SDL_Surface *surf, float yzr) { return (float)surf->h/2 + SCALING_FACTOR*yzr; }

float display_screenx_to_xzr(SDL_Surface *surf, float screenx) { return (-screenx + (float)surf->w/2)/SCALING_FACTOR; }
float display_screeny_to_yzr(SDL_Surface *surf, float screeny) { return (screeny - (float)surf->h/2)/SCALING_FACTOR; }

SDL_Point display_point_to_sdl(SDL_Surface *surf, struct Vec3 pt)
{
	assert(pt.z < 0);
	return (SDL_Point){
		.x = (int)display_xzr_to_screenx(surf, pt.x/pt.z),
		.y = (int)display_yzr_to_screeny(surf, pt.y/pt.z),
	};
}

static void update_minmax(float *min, float *max, float val)
{
	if (val < *min)
		*min = val;
	if (val > *max)
		*max = val;
}

void display_containing_rect(const struct Camera *cam, SDL_Color col, const struct Vec3 *points, unsigned npoints)
{
	if (npoints == 0)
		return;

	// must be in front of the camera, i.e. negative z
	for (unsigned i = 0; i < npoints; i++) {
		if (points[i].z >= 0)
			return;
	}

	float xmin, xmax, ymin, ymax;
	xmin = xmax = display_xzr_to_screenx(cam->surface, points[0].x / points[0].z);
	ymin = ymax = display_yzr_to_screeny(cam->surface, points[0].y / points[0].z);
	for (unsigned i = 1; i < npoints; i++) {
		update_minmax(&xmin, &xmax, display_xzr_to_screenx(cam->surface, points[i].x / points[i].z));
		update_minmax(&ymin, &ymax, display_yzr_to_screeny(cam->surface, points[i].y / points[i].z));
	}

	SDL_FillRect(cam->surface, &(SDL_Rect){
		(int)floorf(xmin),
		(int)floorf(ymin),
		(int)ceilf(xmax-xmin),
		(int)ceilf(ymax-ymin),
	}, SDL_MapRGB(cam->surface->format, col.r, col.g, col.b));
}
