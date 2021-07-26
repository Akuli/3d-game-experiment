#ifndef DELETEMAP_H
#define DELETEMAP_H

#include <SDL2/SDL.h>
#include "map.h"
#include "misc.h"

// returns MISC_STATE_CHOOSER or MISC_STATE_QUIT
enum MiscState deletemap_dialog(struct SDL_Window *wnd, struct Map *maps, int *nmaps, int mapidx);

#endif    // DELETEMAP_H
