#ifndef JUMPER_H
#define JUMPER_H

#include <SDL2/SDL.h>
#include "camera.h"
#include "map.h"

void jumper_init_global_image(const SDL_PixelFormat *pixfmt);

struct Jumper {
	int x, z;
	float y;
};

// Returned rect can be used for drawing the jumper on screen
struct Rect3 jumper_eachframe(struct Jumper *jmp);

// Returns whether jump should begin
bool jumper_press(struct Jumper *jmp, const struct Ellipsoid *el);

// Set y speed to this when jump begins
#define JUMPER_YSPEED 50

#endif  // JUMPER_H
