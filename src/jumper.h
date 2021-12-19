#ifndef JUMPER_H
#define JUMPER_H

#include <SDL2/SDL.h>
#include "camera.h"
#include "map.h"

void jumper_init(const SDL_PixelFormat *pixfmt);
struct Rect3 jumper_get_rect(struct MapCoords loc);

#endif  // JUMPER_H
