#ifndef EDITPLACE_H
#define EDITPLACE_H

#include <SDL2/SDL.h>
#include "place.h"
#include "misc.h"

// Needs array of all places because it's possible to delete a place
enum MiscState editplace_run(SDL_Window *wnd, struct Place *places, int *nplaces, int placeidx);

#endif   // EDITPLACE_H
