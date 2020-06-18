// The event loop that runs when actually playing the game

#ifndef GAME_H
#define GAME_H

#include "place.h"
#include <SDL2/SDL.h>

void game_run(SDL_Window *win, const struct Place *pl);


#endif   // GAME_H
