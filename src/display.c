/*
The mapping from a 3D point (x,y,z) to a screen point (screenx,screeny) is:

	screenx = DISPLAY_WIDTH/2 + DISPLAY_SCALING_FACTOR*x/z
	screeny = DISPLAY_HEIGHT/2 - DISPLAY_SCALING_FACTOR*y/z

Here the positive z direction is where the player is looking.
*/

#include "display.h"
#include <assert.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>   // sudo apt install libsdl2-gfx-dev

float display_xzr_to_screenx(float xzr) { return DISPLAY_WIDTH/2 + DISPLAY_SCALING_FACTOR*xzr; }
float display_yzr_to_screeny(float yzr) { return DISPLAY_HEIGHT/2 - DISPLAY_SCALING_FACTOR*yzr; }

float display_screenx_to_xzr(float screenx) { return (screenx - DISPLAY_WIDTH/2)/DISPLAY_SCALING_FACTOR; }
float display_screeny_to_yzr(float screeny) { return (-screeny + DISPLAY_HEIGHT/2)/DISPLAY_SCALING_FACTOR; }

SDL_Point display_point_to_sdl(struct Vec3 pt)
{
	return (SDL_Point){
		.x = (int)display_xzr_to_screenx(pt.x/pt.z),
		.y = (int)display_yzr_to_screeny(pt.y/pt.z),
	};
}

int compare_y_coords(const void *p1, const void *p2)
{
	const SDL_Point *a = p1;
	const SDL_Point *b = p2;
	return (a->y > b->y) - (a->y < b->y);
}

// smin = source min etc
static int linear_map(int smin, int smax, int dmin, int dmax, int val)
{
	return dmin + (dmax - dmin)*(val - smin)/(smax - smin);
}

static void swap(SDL_Point *a, SDL_Point *b)
{
	SDL_Point tmp = *a;
	*a = *b;
	*b = tmp;
}

static void draw_filled_triangle(SDL_Renderer *rnd, SDL_Point p1, SDL_Point p2, SDL_Point p3)
{
	if (abs(p2.x - p3.x) > abs(p3.y - p2.y)) {
		if (p2.x > p3.x)
			swap(&p2, &p3);
		for (int x = p2.x; x <= p3.x; x++) {
			int y = linear_map(p2.x, p3.x, p2.y, p3.y, x);
			SDL_RenderDrawLine(rnd, p1.x, p1.y, x, y);
		}
	} else {
		if (p2.y > p3.y)
			swap(&p2, &p3);
		for (int y = p2.y; y <= p3.y; y++) {
			int x = linear_map(p2.y, p3.y, p2.x, p3.x, y);
			SDL_RenderDrawLine(rnd, p1.x, p1.y, x, y);
		}
	}
}

#define min(a,b) ((a)<(b) ? (a) : (b))
#define max(a,b) ((a)>(b) ? (a) : (b))
#define min4(a,b,c,d) min(min(a,b),min(c,d))
#define max4(a,b,c,d) max(max(a,b),max(c,d))

void display_4gon(SDL_Renderer *rnd, struct Display4Gon gon)
{
	struct SDL_Point arr[] = {
		display_point_to_sdl(gon.point1),
		display_point_to_sdl(gon.point2),
		display_point_to_sdl(gon.point3),
		display_point_to_sdl(gon.point4),
	};

	//draw_filled_triangle(rnd, arr[0], arr[1], arr[2]);
	//draw_filled_triangle(rnd, arr[3], arr[1], arr[2]);

	int xmin = min4(arr[0].x, arr[1].x, arr[2].x, arr[3].x);
	int xmax = max4(arr[0].x, arr[1].x, arr[2].x, arr[3].x);
	int ymin = min4(arr[0].y, arr[1].y, arr[2].y, arr[3].y);
	int ymax = max4(arr[0].y, arr[1].y, arr[2].y, arr[3].y);
	SDL_RenderFillRect(rnd, &(SDL_Rect){ xmin, ymin, xmax-xmin, ymax-ymin });
}
