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

static uint32_t convert_color_for_gfx(SDL_Color col)
{
	// docs say that it's 0xRRGGBBAA but actual behaviour is 0xAABBGGRR... wtf
	return ((uint32_t)col.a << 24) | ((uint32_t)col.b << 16) | ((uint32_t)col.g << 8) | ((uint32_t)col.r << 0);
}

#define min(a,b) ((a)<(b) ? (a) : (b))
#define max(a,b) ((a)>(b) ? (a) : (b))
#define min4(a,b,c,d) min(min(a,b),min(c,d))
#define max4(a,b,c,d) max(max(a,b),max(c,d))

void display_4gon(SDL_Renderer *rnd, struct Display4Gon gon, SDL_Color col, bool rect)
{
	struct SDL_Point p1 = display_point_to_sdl(gon.point1);
	struct SDL_Point p2 = display_point_to_sdl(gon.point2);
	struct SDL_Point p3 = display_point_to_sdl(gon.point3);
	struct SDL_Point p4 = display_point_to_sdl(gon.point4);

	if (rect){
		int xmin = min4(p1.x, p2.x, p3.x, p4.x);
		int xmax = max4(p1.x, p2.x, p3.x, p4.x);
		int ymin = min4(p1.y, p2.y, p3.y, p4.y);
		int ymax = max4(p1.y, p2.y, p3.y, p4.y);
		SDL_SetRenderDrawColor(rnd, col.r, col.g, col.b, col.a);
		SDL_RenderFillRect(rnd, &(SDL_Rect){ xmin, ymin, xmax-xmin, ymax-ymin });
	}else{
		filledPolygonColor(
			rnd,
			(int16_t[]){ (int16_t)p1.x, (int16_t)p2.x, (int16_t)p4.x, (int16_t)p3.x },
			(int16_t[]){ (int16_t)p1.y, (int16_t)p2.y, (int16_t)p4.y, (int16_t)p3.y },
			4,
			convert_color_for_gfx(col));
	}
}
