#ifndef JUMPER_H
#define JUMPER_H

#include <SDL2/SDL.h>
#include "camera.h"
#include "map.h"

void jumper_init(const SDL_PixelFormat *pixfmt);
void jumper_draw(const struct Camera *cam, struct MapCoords loc);

#endif  // JUMPER_H
