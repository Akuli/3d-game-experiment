#include <SDL2/SDL.h>
#include <limits.h>
#include "../stb/stb_image.h"
#include "camera.h"
#include "log.h"
#include "rect.h"

struct RectImage *rect_load_image(const char *path, const SDL_PixelFormat *pixfmt)
{
	int chansinfile;
	int w, h;
	uint8_t *filedata = stbi_load(path, &w, &h, &chansinfile, 4);
	if (!filedata)
		log_printf_abort("stbi_load failed with path '%s': %s", path, stbi_failure_reason());

	struct RectImage *rectimg = malloc(sizeof(*rectimg) + sizeof(rectimg->data[0])*w*h);
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
	const struct Camera *cam = cache->cam;

	if (!cache->rect->img) {
		// FIXME: transparency
		SDL_FillRect(cam->surface, &(SDL_Rect){xmin,y,xmax-xmin,1}, (uint32_t)0x0000ffffUL);
		return;
	}

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
}
