#ifndef JUMPER_H
#define JUMPER_H

#include <SDL2/SDL.h>
#include "map.h"

// Set y speed to this when jump begins
#define JUMPER_YSPEED 30

void jumper_init_global_image(const SDL_PixelFormat *pixfmt);

struct Jumper {
	int x, z;
	float y;
};

// Returned rect can be used for drawing the jumper on screen
struct Rect3 jumper_eachframe(struct Jumper *jmp);

// May begin a jump by changing el->jumpstate
void jumper_press(struct Jumper *jmp, struct Ellipsoid *el);

#endif  // JUMPER_H
