#include <SDL2/SDL.h>
#include <limits.h>
#include "../stb/stb_image.h"
#include "camera.h"
#include "log.h"
#include "rect.h"

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

	cache->ymin = INT_MAX;
	cache->ymax = INT_MIN;
	for (int c = 0; c < 4; c++) {
		cache->ymin = min(cache->ymin, (int)cache->screencorners[c].y);
		cache->ymax = max(cache->ymax, (int)cache->screencorners[c].y);
	}

	return true;
}

bool rect_xminmax(const struct RectCache *cache, int y, int *xmin, int *xmax)
{
	if (y < cache->ymin || y > cache->ymax)
		return false;

	int n = 0;
	Vec2 inters[4];

	for (int c = 0; c < 4; c++) {
		if (intersect_line_segments(
			cache->screencorners[c], cache->screencorners[(c+1)%4],
			(Vec2){0,y}, (Vec2){1,y}, true,
			&inters[n]))
		{
			n++;
		}
	}

	// TODO: fails sometimes
	//SDL_assert(n <= 2);
	if (n != 2)
		return false;
	*xmin = (int)ceilf(min(inters[0].x, inters[1].x));
	*xmax = (int)      max(inters[0].x, inters[1].x);
	clamp(xmin, 0, cache->cam->surface->w);
	clamp(xmax, 0, cache->cam->surface->w);
	return (*xmin < *xmax);
}

void rect_drawrow(const struct RectCache *cache, int y, int xmin, int xmax)
{
	// FIXME: transparency
	const struct Camera *cam = cache->cam;
	SDL_FillRect(cam->surface, &(SDL_Rect){xmin,y,xmax-xmin,1}, (uint32_t)0x0000ffffUL);
	return;
}
