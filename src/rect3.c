#include "rect3.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "camera.h"
#include "linalg.h"
#include "misc.h"

bool rect3_visible_fillcache(const struct Rect3 *r, const struct Camera *cam, struct Rect3Cache *cache)
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
		break;
	next_corner:
		continue;
	}
	if (!anycornervisible)
		return false;

	cache->rect = r;
	cache->cam = cam;
	for (int c = 0; c < 4; c++)
		cache->screencorners[c] = camera_point_cam2screen(cam, camera_point_world2cam(cam, r->corners[c]));

	SDL_Point points[] = {
		{ (int)cache->screencorners[0].x, (int)cache->screencorners[0].y },
		{ (int)cache->screencorners[1].x, (int)cache->screencorners[1].y },
		{ (int)cache->screencorners[2].x, (int)cache->screencorners[2].y },
		{ (int)cache->screencorners[3].x, (int)cache->screencorners[3].y },
	};
	SDL_Rect camrect = { 0, 0, cam->surface->w, cam->surface->h };
	SDL_Rect tmp;

	// clip argument of SDL_EnclosePoints doesn't work like i want
	return SDL_EnclosePoints(points, 4, NULL, &tmp)
		&& SDL_IntersectRect(&tmp, &camrect, &cache->bbox);
}

bool rect3_xminmax(const struct Rect3Cache *cache, int y, int *xmin, int *xmax)
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

void rect3_drawrow(const struct Rect3Cache *cache, int y, int xmin, int xmax)
{
	SDL_Surface *surf = cache->cam->surface;
	SDL_assert(surf->pitch % sizeof(uint32_t) == 0);
	int mypitch = surf->pitch / sizeof(uint32_t);
	uint32_t *start = (uint32_t *)surf->pixels + y*mypitch + xmin;
	uint32_t *end   = (uint32_t *)surf->pixels + y*mypitch + xmax;

	// rgb_average seems to perform better when one argument is compile-time known
	const SDL_PixelFormat *f = surf->format;
	SDL_assert(f->Rmask == 0xff0000 && f->Gmask == 0x00ff00 && f->Bmask == 0x0000ff);

	if (cache->rect->highlight) {
		for (uint32_t *ptr = start; ptr <= end; ptr++)
			*ptr = rgb_average(*ptr, 0xff0000);
	} else {
		for (uint32_t *ptr = start; ptr <= end; ptr++)
			*ptr = rgb_average(*ptr, 0x00ffff);
	}
}

float rect3_get_camcoords_z(const struct Rect3 *r, const struct Camera *cam, float xzr, float yzr)
{
	Vec3 start = camera_point_world2cam(cam, r->corners[1]);
	Vec3 v = mat3_mul_vec3(cam->world2cam, vec3_sub(r->corners[1], r->corners[0]));
	Vec3 w = mat3_mul_vec3(cam->world2cam, vec3_sub(r->corners[1], r->corners[2]));

	/*
	start + a*v + b*w = z*(xzr,yzr,1)

	As a matrix:
		 _             _   _  _
		| xzr  v.x  w.x | | z  |
		| yzr  v.y  w.y | | -a | = start
		|_ 1   v.z  w.z_| |_-b_|

	Now z can be solved with Cramer's rule.
	*/
	float numer = mat3_det((Mat3){ .rows = {
		{ start.x, v.x, w.x },
		{ start.y, v.y, w.y },
		{ start.z, v.z, w.z },
	}});
	float denom = mat3_det((Mat3){ .rows = {
		{ xzr, v.x, w.x },
		{ yzr, v.y, w.y },
		{  1 , v.z, w.z },
	}});
	return numer/denom;
}


static void draw_2d_rect(SDL_Surface *surf, SDL_Rect r)
{
	if (r.w < 0) {
		r.w = abs(r.w);
		r.x -= r.w;
	}
	if (r.h < 0) {
		r.h = abs(r.h);
		r.y -= r.h;
	}

	SDL_Rect clip;
	if (SDL_IntersectRect(&r, &(SDL_Rect){0,0,surf->w,surf->h}, &clip)) {
		uint32_t color = SDL_MapRGB(surf->format, 0xff, 0x00, 0x00);
		SDL_FillRect(surf, &clip, color);
	}
}

static void swap(int *a, int *b)
{
	int tmp = *a;
	*a = *b;
	*b = tmp;
}

static void draw_line(SDL_Surface *surf, Vec2 start, Vec2 end)
{
	int x1 = (int)start.x;
	int y1 = (int)start.y;
	int x2 = (int)end.x;
	int y2 = (int)end.y;

	if (x1 == x2) {
		// Vertical line
		draw_2d_rect(surf, (SDL_Rect){ x1-1, y1, 3, y2-y1 });
	} else if (y1 == y2) {
		// Horizontal line
		draw_2d_rect(surf, (SDL_Rect){ x1, y1-1, x2-x1, 3 });
	} else if (abs(y2-y1) > abs(x2-x1)) {
		// Many vertical lines
		if (x1 > x2) { swap(&x1, &x2); swap(&y1, &y2); }
		for (int x = x1; x <= x2; x++) {
			int y     = y1 + (y2 - y1)*(x   - x1)/(x2 - x1);
			int ynext = y1 + (y2 - y1)*(x+1 - x1)/(x2 - x1);
			clamp(&ynext, min(y1,y2), max(y1,y2));
			draw_2d_rect(surf, (SDL_Rect){ x-1, y, 3, ynext-y });
		}
	} else {
		// Many horizontal lines
		if (y1 > y2) { swap(&x1, &x2); swap(&y1, &y2); }
		for (int y = y1; y <= y2; y++) {
			int x     = x1 + (x2 - x1)*(y   - y1)/(y2 - y1);
			int xnext = x1 + (x2 - x1)*(y+1 - y1)/(y2 - y1);
			clamp(&xnext, min(x1,x2), max(x1,x2));
			draw_2d_rect(surf, (SDL_Rect){ x, y-1, xnext-x, 3 });
		}
	}
}

void rect3_drawborder(const struct Rect3 *r, const struct Camera *cam)
{
	struct Rect3Cache rcache;
	if (!rect3_visible_fillcache(r, cam, &rcache))
		return;

	draw_line(cam->surface, rcache.screencorners[0], rcache.screencorners[1]);
	draw_line(cam->surface, rcache.screencorners[1], rcache.screencorners[2]);
	draw_line(cam->surface, rcache.screencorners[2], rcache.screencorners[3]);
	draw_line(cam->surface, rcache.screencorners[3], rcache.screencorners[0]);
}
