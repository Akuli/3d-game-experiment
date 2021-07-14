#ifndef EDITPLACE_H
#define EDITPLACE_H

#include <SDL2/SDL.h>
#include "place.h"
#include "misc.h"

enum MiscState editplace_run(SDL_Window *wnd, struct Place *pl);

#endif   // EDITPLACE_H
