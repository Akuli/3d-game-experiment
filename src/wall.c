#include "wall.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <SDL2/SDL.h>
#include "camera.h"
#include "mathstuff.h"
#include "player.h"
#include "misc.h"


static float linear_map(float srcmin, float srcmax, float dstmin, float dstmax, float val)
{
	float ratio = (val - srcmin)/(srcmax - srcmin);
	return dstmin + ratio*(dstmax - dstmin);
}

void wall_init(struct Wall *w)
{
	w->top1 = w->top2 = (Vec3){ (float)w->startx, WALL_Y_MAX, (float)w->startz };
	w->bot1 = w->bot2 = (Vec3){ (float)w->startx, WALL_Y_MIN, (float)w->startz };

	switch(w->dir) {
		case WALL_DIR_XY:
			w->top2.x += 1.0f;
			w->bot2.x += 1.0f;
			break;
		case WALL_DIR_ZY:
			w->top2.z += 1.0f;
			w->bot2.z += 1.0f;
			break;
	}
}

bool wall_match(const struct Wall *w1, const struct Wall *w2)
{
	return w1->dir == w2->dir && w1->startx == w2->startx && w1->startz == w2->startz;
}


static bool wall_is_visible(const struct Wall *w, const struct Camera *cam)
{
	Vec3 corners[] = { w->top1, w->top2, w->bot1, w->bot2 };

	// Ensure that no corner is behind camera. This means that x/z ratios will work.
	for (int c = 0; c < 4; c++)
		if (!plane_whichside(cam->visplanes[CAMERA_CAMPLANE_IDX], corners[c]))
			return false;

	// check if any corner is visible
	for (int c = 0; c < 4; c++) {
		bool cornervisible = true;
		for (int v = 0; v < sizeof(cam->visplanes)/sizeof(cam->visplanes[0]); v++) {
			if (!plane_whichside(cam->visplanes[v], corners[c])) {
				cornervisible = false;
				break;
			}
		}

		if (cornervisible)
			return true;
	}
	return false;
}

Vec3 wall_center(const struct Wall *w)
{
	float x = (float)w->startx;
	float y = (WALL_Y_MIN + WALL_Y_MAX)/2;
	float z = (float)w->startz;

	switch(w->dir) {
		case WALL_DIR_XY: x += 0.5f; break;
		case WALL_DIR_ZY: z += 0.5f; break;
	}

	return (Vec3){x,y,z};
}

bool wall_side(const struct Wall *w, Vec3 pt)
{
	Vec3 center = wall_center(w);
	switch(w->dir) {
		case WALL_DIR_XY: return (center.z < pt.z);
		case WALL_DIR_ZY: return (center.x < pt.x);
	}

	// never actually runs, but makes compiler happy
	return false;
}

extern inline bool wall_linedup(const struct Wall *w1, const struct Wall *w2);

static void draw_rect(SDL_Surface *surf, SDL_Rect r)
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
		draw_rect(surf, (SDL_Rect){ x1-1, y1, 3, y2-y1 });
	} else if (y1 == y2) {
		// Horizontal line
		draw_rect(surf, (SDL_Rect){ x1, y1-1, x2-x1, 3 });
	} else if (abs(y2-y1) > abs(x2-x1)) {
		// Many vertical lines
		if (x1 > x2) { swap(&x1, &x2); swap(&y1, &y2); }
		for (int x = x1; x <= x2; x++) {
			int y     = y1 + (y2 - y1)*(x   - x1)/(x2 - x1);
			int ynext = y1 + (y2 - y1)*(x+1 - x1)/(x2 - x1);
			clamp(&ynext, min(y1,y2), max(y1,y2));
			draw_rect(surf, (SDL_Rect){ x-1, y, 3, ynext-y });
		}
	} else {
		// Many horizontal lines
		if (y1 > y2) { swap(&x1, &x2); swap(&y1, &y2); }
		for (int y = y1; y <= y2; y++) {
			int x     = x1 + (x2 - x1)*(y   - y1)/(y2 - y1);
			int xnext = x1 + (x2 - x1)*(y+1 - y1)/(y2 - y1);
			clamp(&xnext, min(x1,x2), max(x1,x2));
			draw_rect(surf, (SDL_Rect){ x, y-1, xnext-x, 3 });
		}
	}
}

void wall_drawborder(const struct Wall *w, const struct Camera *cam)
{
	if (!wall_is_visible(w, cam))
		return;

	/*
	Can't use wall_visible_xminmax_fillcache(), it gives weird corner case where border
	disappears when looking along wall
	*/
	Vec2 top1 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->top1));
	Vec2 top2 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->top2));
	Vec2 bot1 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->bot1));
	Vec2 bot2 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->bot2));

	draw_line(cam->surface, bot1, bot2);
	draw_line(cam->surface, bot2, top2);
	draw_line(cam->surface, top2, top1);
	draw_line(cam->surface, top1, bot1);
}

bool wall_visible_xminmax_fillcache(
	const struct Wall *w, const struct Camera *cam,
	int *xmin, int *xmax,
	struct WallCache *wc)
{
	if (!wall_is_visible(w, cam)) {
		// Can't fill cache in this case
		return false;
	}

	*wc = (struct WallCache){
		.wall = w,
		.cam = cam,
		.top1 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->top1)),
		.top2 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->top2)),
		.bot1 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->bot1)),
		.bot2 = camera_point_cam2screen(cam, camera_point_world2cam(cam, w->bot2)),
	};

	SDL_assert(fabsf(wc->top1.x - wc->bot1.x) < 1e-5f);
	SDL_assert(fabsf(wc->top2.x - wc->bot2.x) < 1e-5f);

	// need only top corners because others have same screen x
	*xmin = (int)ceilf(min(wc->top1.x, wc->top2.x));
	*xmax = (int)      max(wc->top1.x, wc->top2.x);
	return (*xmin <= *xmax);
}

void wall_yminmax(const struct WallCache *wc, int x, int *ymin, int *ymax)
{
	*ymin = (int) linear_map(wc->top1.x, wc->top2.x, wc->top1.y, wc->top2.y, x);
	*ymax = (int) linear_map(wc->bot1.x, wc->bot2.x, wc->bot1.y, wc->bot2.y, x);

	clamp(ymin, 0, wc->cam->surface->h - 1);
	clamp(ymax, 0, wc->cam->surface->h - 1);
}

void wall_drawcolumn(const struct WallCache *wc, int x, int ymin, int ymax, bool highlight)
{
	SDL_Surface *surf = wc->cam->surface;

	SDL_assert(surf->pitch % sizeof(uint32_t) == 0);
	int mypitch = surf->pitch / sizeof(uint32_t);

	uint32_t *start = (uint32_t *)surf->pixels + ymin*mypitch + x;
	uint32_t *end   = (uint32_t *)surf->pixels + ymax*mypitch + x;

	// rgb_average seems to perform better when one argument is compile-time known

	const SDL_PixelFormat *f = surf->format;
	SDL_assert(f->Rmask == 0xff0000 && f->Gmask == 0x00ff00 && f->Bmask == 0x0000ff);

	if (highlight) {
		for (uint32_t *ptr = start; ptr < end; ptr += mypitch)
			*ptr = misc_rgb_average(*ptr, 0xff0000);
	} else {
		for (uint32_t *ptr = start; ptr < end; ptr += mypitch)
			*ptr = misc_rgb_average(*ptr, 0x00ffff);
	}
}
