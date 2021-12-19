#include "jumper.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include <stdint.h>
#include "../stb/stb_image.h"
#include "camera.h"
#include "log.h"
#include "rect.h"

static struct RectImage *jumper_image = NULL;

static void free_image(void)
{
	free(jumper_image);
}

void jumper_init(const SDL_PixelFormat *pixfmt)
{
	SDL_assert(jumper_image == NULL);
	jumper_image = rect_load_image("assets/jumper.png", pixfmt);
	atexit(free_image);
}

void jumper_draw(const struct Camera *cam, struct MapCoords loc)
{
	SDL_assert(jumper_image != NULL);
	struct Rect r = {
		.corners = {
			{loc.x,0,loc.z},
			{loc.x,0,loc.z+1},
			{loc.x+1,0,loc.z+1},
			{loc.x+1,0,loc.z},
		},
		.img = jumper_image,
	};
	struct RectCache rc;
	if (rect_visible_fillcache(&r, cam, &rc)) {
		for (int y = 0; y < cam->surface->h; y++) {
			int xmin, xmax;
			if (rect_xminmax(&rc, y, &xmin, &xmax))
				rect_drawrow(&rc, y, xmin, xmax);
		}
	}
}
