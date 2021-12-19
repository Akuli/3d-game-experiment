#include "jumper.h"
#include <SDL2/SDL.h>
#include <limits.h>
#include <stdint.h>
#include "camera.h"
#include "log.h"
#include "rect3.h"

static struct Rect3Image *jumper_image = NULL;

static void free_image(void)
{
	free(jumper_image);
}

void jumper_init(const SDL_PixelFormat *pixfmt)
{
	SDL_assert(jumper_image == NULL);
	jumper_image = rect3_load_image("assets/jumper.png", pixfmt);
	atexit(free_image);
}

struct Rect3 jumper_get_rect(struct MapCoords loc)
{
	SDL_assert(jumper_image != NULL);
	return (struct Rect3){
		.corners = {
			{ loc.x, 0, loc.z },
			{ loc.x, 0, loc.z+1 },
			{ loc.x+1, 0, loc.z+1 },
			{ loc.x+1, 0, loc.z },
		},
		.img = jumper_image,
	};
}
