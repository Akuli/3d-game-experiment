#include "rect3.h"
#include "../stb/stb_image.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include "camera.h"
#include "linalg.h"
#include "misc.h"
#include "log.h"

struct Rect3Image *rect3_load_image(const char *path, const SDL_PixelFormat *pixfmt)
{
	int chansinfile;
	int w, h;
	uint8_t *filedata = stbi_load(path, &w, &h, &chansinfile, 4);
	if (!filedata)
		log_printf_abort("stbi_load failed with path '%s': %s", path, stbi_failure_reason());

	struct Rect3Image *rectimg = malloc(sizeof(*rectimg) + sizeof(rectimg->data[0])*w*h);
	if (!rectimg)
		log_printf_abort("not enough mem");

	rectimg->width = w;
	rectimg->height = h;
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int i = w*y + x;
			if (filedata[4*i+3] < 0x80)
				rectimg->data[i] = ~(uint32_t)0;  // special value for transparent
			else
				rectimg->data[i] = SDL_MapRGB(pixfmt, filedata[4*i], filedata[4*i+1], filedata[4*i+2]);
		}
	}

	stbi_image_free(filedata);
	return rectimg;
}

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
	// TODO: put common code of two cases into here
	if (cache->rect->img) {
		const struct Camera *cam = cache->cam;

		SDL_assert(cache->rect->img);
		Vec3 A = camera_point_world2cam(cam, cache->rect->corners[0]);
		Vec3 B = camera_point_world2cam(cam, cache->rect->corners[2]);
		Vec3 C = camera_point_world2cam(cam, cache->rect->corners[1]);
		Vec3 v = vec3_sub(A, C);
		Vec3 w = vec3_sub(B, C);

		float yzr = camera_screeny_to_yzr(cache->cam, y);

		/*
		We project a ray from the camera towards a vector "dir" onto the rectangle.
		In camera coordinates:

			C + av + bw = t(xzr, yzr, 1)

		With matrices:
			 _              _   _ _     _    _
			| -xzr  v.x  w.x | | t |   | -C.x |
			| -yzr  v.y  w.y | | a | = | -C.y |
			|_ -1   v.z  w.z_| |_b_|   |_-C.z_|

		We can solve a and b with Cramer's rule,

			a = det(Ma)/det(M), b = det(Mb)/det(M)

		where M is the matrix above, and
			      _               _
			     | -xzr  -C.x  w.x |
			Ma = | -yzr  -C.y  w.y |
			     |_ -1   -C.z  w.z_|
			      _               _
			     | -xzr  v.x  -C.x |
			Mb = | -yzr  v.y  -C.y |
			     |_ -1   v.z  -C.z_|

		To prevent recomputing everything in a loop where only xzr varies, each determinant
		is split into two parts by linearity before the loop begins.
		*/
		float detM_xzrcoeff = mat3_det((Mat3){ .rows = {
			{ -1, v.x, w.x },
			{ 0,  v.y, w.y },
			{ 0,  v.z, w.z },
		}});
		float detM_noxzr = mat3_det((Mat3){ .rows = {
			{ 0,    v.x, w.x },
			{ -yzr, v.y, w.y },
			{ -1,   v.z, w.z },
		}});
		float detMa_xzrcoeff = mat3_det((Mat3){ .rows = {
			{ -1, -C.x, w.x },
			{ 0,  -C.y, w.y },
			{ 0,  -C.z, w.z },
		}});
		float detMa_noxzr = mat3_det((Mat3){ .rows = {
			{ 0,    -C.x, w.x },
			{ -yzr, -C.y, w.y },
			{ -1,   -C.z, w.z },
		}});
		float detMb_xzrcoeff = mat3_det((Mat3){ .rows = {
			{ -1, v.x, -C.x },
			{ 0,  v.y, -C.y },
			{ 0,  v.z, -C.z },
		}});
		float detMb_noxzr = mat3_det((Mat3){ .rows = {
			{ 0,    v.x, -C.x },
			{ -yzr, v.y, -C.y },
			{ -1,   v.z, -C.z },
		}});

		int width = cache->rect->img->width;
		int height = cache->rect->img->height;
		const uint32_t *pxsrc = cache->rect->img->data;

		SDL_assert(cam->surface->pitch % sizeof(uint32_t) == 0);
		int mypitch = cam->surface->pitch / sizeof(uint32_t);
		uint32_t *pxdst = (uint32_t *)cam->surface->pixels + mypitch*y + xmin;

		int xdiff = xmax-xmin;

		// Ugly code, but vectorization friendly. Measurable perf improvement.
#define LOOP for (int i = 0; i < xdiff; i++)
#define ARRAY(T, Name) T Name[CAMERA_SCREEN_WIDTH]; LOOP Name[i]
		ARRAY(float, xzr) = camera_screenx_to_xzr(cache->cam, xmin+i);
		ARRAY(float, detM) = xzr[i]*detM_xzrcoeff + detM_noxzr;
		ARRAY(float, a) = (xzr[i]*detMa_xzrcoeff + detMa_noxzr)/detM[i];
		ARRAY(float, b) = (xzr[i]*detMb_xzrcoeff + detMb_noxzr)/detM[i];
		ARRAY(int, picx) = (int)(a[i]*width);
		ARRAY(int, picy) = (int)(b[i]*height);
		LOOP clamp(&picx[i], 0, width-1);
		LOOP clamp(&picy[i], 0, height-1);
		ARRAY(int, idx) = width*picy[i] + picx[i];
		ARRAY(uint32_t, px) = pxsrc[idx[i]];
		LOOP if (px[i] != ~(uint32_t)0) pxdst[i] = px[i];
#undef LOOP
#undef ARRAY
	} else {
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
