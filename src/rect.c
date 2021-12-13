#include "rect.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include "../stb/stb_image.h"
#include "camera.h"
#include "intersections.h"
#include "log.h"
#include "misc.h"

bool rect_visible_fillcache(const struct Rect *r, const struct Camera *cam, struct RectCache *cache)
{
	// Ensure that no corner is behind camera. This means that x/z and y/z ratios will work.
	for (int c = 0; c < 4; c++)
		if (!plane_whichside(cam->visplanes[CAMERA_CAMPLANE_IDX], r->corners[c]))
			return false;

	bool anycornervisible = false;
	for (int c = 0; c < 4; c++) {
		for (int v = 0; v < sizeof(cam->visplanes)/sizeof(cam->visplanes[0]); v++) {
			if (!plane_whichside(cam->visplanes[v], r->corners[c]))
				goto next_corner;
		}
		anycornervisible = true;
	next_corner:
		continue;
	}
	if (!anycornervisible)
		return false;

	cache->rect = r;
	cache->cam = cam;
	for (int c = 0; c < 4; c++)
		cache->screencorners[c] = camera_point_cam2screen(cam, camera_point_world2cam(cam, r->corners[c]));

	// clip argument of SDL_EnclosePoints doesn't work like i want
	SDL_Rect tmp;
	return SDL_EnclosePoints(
		(SDL_Point[]){
			{ (int)cache->screencorners[0].x, (int)cache->screencorners[0].y },
			{ (int)cache->screencorners[1].x, (int)cache->screencorners[1].y },
			{ (int)cache->screencorners[2].x, (int)cache->screencorners[2].y },
			{ (int)cache->screencorners[3].x, (int)cache->screencorners[3].y },
		},
		4, NULL, &tmp
	) && SDL_IntersectRect(
		&tmp, &(SDL_Rect){ 0, 0, cam->surface->w, cam->surface->h }, &cache->bbox
	);
}

bool rect_xminmax(const struct RectCache *cache, int y, int *xmin, int *xmax)
{
	if (!(cache->bbox.y <= y && y < cache->bbox.y+cache->bbox.h))
		return false;

	int n = 0;
	float interx[4];

	Vec2 corner1 = cache->screencorners[3];
	for (int c = 0; c < 4; c++) {
		Vec2 corner2 = cache->screencorners[c];
		if (fabsf(corner1.y - corner2.y) > 1e-5f &&
			((corner1.y <= y && y <= corner2.y) || (corner1.y >= y && y >= corner2.y)))
		{
			float t = (y - corner1.y) / (corner2.y - corner1.y);
			interx[n++] = corner1.x + t*(corner2.x - corner1.x);
		}
		corner1 = corner2;
	}

	// There are n=3 intersections when a line goes through corner of wall
	if (n < 2)
		return false;

	*xmin = INT_MAX;
	*xmax = INT_MIN;
	for (int i = 0; i < n; i++) {
		*xmin = min(*xmin, (int)ceilf(interx[i]));
		*xmax = max(*xmax, (int)      interx[i] );
	}
	clamp(xmin, 0, cache->cam->surface->w-1);
	clamp(xmax, 0, cache->cam->surface->w-1);
	return (*xmin <= *xmax);
}

void rect_drawrow(const struct RectCache *cache, int y, int xmin, int xmax, bool highlight)
{
	SDL_Surface *surf = cache->cam->surface;
	SDL_assert(surf->pitch % sizeof(uint32_t) == 0);
	int mypitch = surf->pitch / sizeof(uint32_t);
	uint32_t *start = (uint32_t *)surf->pixels + y*mypitch + xmin;
	uint32_t *end   = (uint32_t *)surf->pixels + y*mypitch + xmax;

	// rgb_average seems to perform better when one argument is compile-time known
	const SDL_PixelFormat *f = surf->format;
	SDL_assert(f->Rmask == 0xff0000 && f->Gmask == 0x00ff00 && f->Bmask == 0x0000ff);

	if (highlight) {
		for (uint32_t *ptr = start; ptr <= end; ptr++)
			*ptr = misc_rgb_average(*ptr, 0xff0000);
	} else {
		for (uint32_t *ptr = start; ptr <= end; ptr++)
			*ptr = misc_rgb_average(*ptr, 0x00ffff);
	}
}
