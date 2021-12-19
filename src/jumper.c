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
